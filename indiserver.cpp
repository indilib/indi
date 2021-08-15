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

#define _GNU_SOURCE // needed for siginfo_t and sigaction

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


#define INDIPORT      7624    /* default TCP/IP port to listen */
#define REMOTEDVR     (-1234) /* invalid PID to flag remote drivers */
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

class Msg {
public:
    int count;         /* number of consumers left */
    unsigned long cl;  /* content length */
    char *cp;          /* content: buf or malloced */

    Msg();
    ~Msg();
    void alloc(unsigned long cl);

    /* save str as content in mp. */
    void setFromStr(const char * str);

    /* print root as content in mp.*/
    void setFromXMLEle(XMLEle *root);
};

class MsgQueue {
    void unrefMsg(Msg * msg);
public:
    std::list<Msg*> msgq;           /* Msg queue */
    unsigned int nsent; /* bytes of current Msg sent so far */

    MsgQueue();
    ~MsgQueue();

    void pushMsg(Msg * msg);

    /* return storage size of all Msqs on the given q */
    unsigned long msgQSize() const;

    Msg * headMsg() const;
    void consumeHeadMsg();

    /* Remove all messages from queue */
    void clearMsgQueue();
};

/* device + property name */
class Property {
public:
    char dev[MAXINDIDEVICE];
    char name[MAXINDINAME];
    BLOBHandling blob; /* when to snoop BLOBs */
};

class Fifo {
public:
    std::string name; /* Path to FIFO for dynamic startups & shutdowns of drivers */
    int fd = -1;
};

static Fifo * fifo = nullptr;


/* info for each connected client */
class ClInfo: public MsgQueue {
public:
    std::list<Property*> props; /* props we want */
    int allprops;       /* saw getProperties w/o device */
    BLOBHandling blob;  /* when to send setBLOBs */
    int s;              /* socket for this client */
    LilXML *lp;         /* XML parsing context */

    ClInfo();
    ~ClInfo();
};
static std::set<ClInfo*> clients;

/* info for each connected driver */
class DvrInfo: public MsgQueue
{
public:
    std::string name; /* persistent name */
    char envDev[MAXSBUF];
    char envConfig[MAXSBUF];
    char envSkel[MAXSBUF];
    char envPrefix[MAXSBUF];
    char host[MAXSBUF];
    int port;
    //char dev[MAXINDIDEVICE];		/* device served by this driver */
    char **dev;         /* device served by this driver */
    int ndev;           /* number of devices served by this driver */
    std::list<Property*>sprops;   /* props we snoop */
    int pid;            /* process id or REMOTEDVR if remote */
    int rfd;            /* read pipe fd */
    int wfd;            /* write pipe fd */
    int efd;            /* stderr from driver, if local */
    int restarts;       /* times process has been restarted */
    LilXML *lp;         /* XML parsing context */

    DvrInfo();
    ~DvrInfo();
};
static std::set<DvrInfo*> drivers;


static const char *me;                                 /* our name */
static int port = INDIPORT;                            /* public INDI port */
static int verbose;                                    /* chattiness */
static int lsocket;                                    /* listen socket */
static char *ldir;                                     /* where to log driver messages */
static unsigned int maxqsiz  = (DEFMAXQSIZ * 1024 * 1024); /* kill if these bytes behind */
static unsigned int maxstreamsiz  = (DEFMAXSSIZ * 1024 * 1024); /* drop blobs if these bytes behind while streaming*/
static int maxrestarts   = DEFMAXRESTART;

static void logStartup(int ac, char *av[]);
static void usage(void);
//static void noZombies(void);
static void reapZombies(void);
static void noSIGPIPE(void);
static void indiFIFO(void);
static void indiRun(void);
static void indiListen(void);
static void newFIFO(void);
static void newClient(void);
static int newClSocket(void);
static void shutdownClient(ClInfo *cp);
static int readFromClient(ClInfo *cp);
static void startDvr(DvrInfo *dp);
static void startLocalDvr(DvrInfo *dp);
static void startRemoteDvr(DvrInfo *dp);
static int openINDIServer(char host[], int indi_port);
static void shutdownDvr(DvrInfo *dp, int restart);
static int isDeviceInDriver(const char *dev, DvrInfo *dp);
static void q2RDrivers(const char *dev, Msg *mp, XMLEle *root);
static void q2SDrivers(DvrInfo *me, int isblob, const char *dev, const char *name, Msg *mp, XMLEle *root);
static int q2Clients(ClInfo *notme, int isblob, const char *dev, const char *name, Msg *mp, XMLEle *root);
static int q2Servers(DvrInfo *me, Msg *mp, XMLEle *root);
static void addSDevice(DvrInfo *dp, const char *dev, const char *name);
static Property *findSDevice(DvrInfo *dp, const char *dev, const char *name);
static void addClDevice(ClInfo *cp, const char *dev, const char *name, int isblob);
static int findClDevice(ClInfo *cp, const char *dev, const char *name);
static int readFromDriver(DvrInfo *dp);
static int stderrFromDriver(DvrInfo *dp);
static int sendClientMsg(ClInfo *cp);
static int sendDriverMsg(DvrInfo *cp);
static void crackBLOB(const char *enableBLOB, BLOBHandling *bp);
static void crackBLOBHandling(const char *dev, const char *name, const char *enableBLOB, ClInfo *cp);
static void traceMsg(XMLEle *root);
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
                    fifo = new Fifo();
                    fifo->name = *++av;
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
    /*noZombies();*/
    reapZombies();
    noSIGPIPE();

    /* start each driver */
    while (ac-- > 0)
    {
        DvrInfo * dr = new DvrInfo();
        dr->name = *av++;
    }

    /* announce we are online */
    indiListen();

    /* Load up FIFO, if available */
    indiFIFO();

    /* handle new clients and all io */
    while (1)
        indiRun();

    /* whoa! */
    fprintf(stderr, "unexpected return from main\n");
    return (1);
}

/* record we have started and our args */
static void logStartup(int ac, char *av[])
{
    int i;

    fprintf(stderr, "%s: startup: ", indi_tstamp(NULL));
    for (i = 0; i < ac; i++)
        fprintf(stderr, "%s ", av[i]);
    fprintf(stderr, "\n");
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

/* arrange for no zombies if drivers die */
//static void noZombies()
//{
//    struct sigaction sa;
//    sa.sa_handler = SIG_IGN;
//    sigemptyset(&sa.sa_mask);
//#ifdef SA_NOCLDWAIT
//    sa.sa_flags = SA_NOCLDWAIT;
//#else
//    sa.sa_flags = 0;
//#endif
//    (void)sigaction(SIGCHLD, &sa, NULL);
//}

/* reap zombies when drivers die, in order to leave SIGCHLD unmodified for subprocesses */
static void zombieRaised(int signum, siginfo_t *sig, void *data)
{
    INDI_UNUSED(data);
    switch (signum)
    {
        case SIGCHLD:
            fprintf(stderr, "Child process %d died\n", sig->si_pid);
            waitpid(sig->si_pid, NULL, WNOHANG);
            break;

        default:
            fprintf(stderr, "Received unexpected signal %d\n", signum);
    }
}

/* reap zombies as they die */
static void reapZombies()
{
    struct sigaction sa;
    sa.sa_sigaction = zombieRaised;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    (void)sigaction(SIGCHLD, &sa, NULL);
}

/* turn off SIGPIPE on bad write so we can handle it inline */
static void noSIGPIPE()
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGPIPE, &sa, NULL);
}

/* start the given INDI driver process or connection.
 * exit if trouble.
 */
static void startDvr(DvrInfo *dp)
{
    if (dp->name == "@")
        startRemoteDvr(dp);
    else
        startLocalDvr(dp);
}

/* start the given local INDI driver process.
 * exit if trouble.
 */
static void startLocalDvr(DvrInfo *dp)
{
    Msg *mp;
    char buf[32];
    int rp[2], wp[2], ep[2];
    int pid;

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "STARTING \"%s\"\n", dp->name.c_str());
    fflush(stderr);
#endif

    /* build three pipes: r, w and error*/
    if (pipe(rp) < 0)
    {
        fprintf(stderr, "%s: read pipe: %s\n", indi_tstamp(NULL), strerror(errno));
        Bye();
    }
    if (pipe(wp) < 0)
    {
        fprintf(stderr, "%s: write pipe: %s\n", indi_tstamp(NULL), strerror(errno));
        Bye();
    }
    if (pipe(ep) < 0)
    {
        fprintf(stderr, "%s: stderr pipe: %s\n", indi_tstamp(NULL), strerror(errno));
        Bye();
    }

    /* fork&exec new process */
    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "%s: fork: %s\n", indi_tstamp(NULL), strerror(errno));
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
            (void)close(fd);

        if (*dp->envDev)
            setenv("INDIDEV", dp->envDev, 1);
        /* Only reset environment variable in case of FIFO */
        else if (fifo)
            unsetenv("INDIDEV");
        if (*dp->envConfig)
            setenv("INDICONFIG", dp->envConfig, 1);
        else if (fifo)
            unsetenv("INDICONFIG");
        if (*dp->envSkel)
            setenv("INDISKEL", dp->envSkel, 1);
        else if (fifo)
            unsetenv("INDISKEL");
        char executable[MAXSBUF];
        if (*dp->envPrefix)
        {
            setenv("INDIPREFIX", dp->envPrefix, 1);
#if defined(OSX_EMBEDED_MODE)
            snprintf(executable, MAXSBUF, "%s/Contents/MacOS/%s", dp->envPrefix, dp->name);
#elif defined(__APPLE__)
            snprintf(executable, MAXSBUF, "%s/%s", dp->envPrefix, dp->name);
#else
            snprintf(executable, MAXSBUF, "%s/bin/%s", dp->envPrefix, dp->name.c_str());
#endif

            fprintf(stderr, "%s\n", executable);

            execlp(executable, dp->name.c_str(), NULL);
        }
        else
        {
            if (fifo)
                unsetenv("INDIPREFIX");
            if (dp->name[0] == '.')
            {
                snprintf(executable, MAXSBUF, "%s/%s", dirname((char*)me), dp->name.c_str());
                execlp(executable, dp->name.c_str(), NULL);
            }
            else
            {
                execlp(dp->name.c_str(), dp->name.c_str(), NULL);
            }
        }

#ifdef OSX_EMBEDED_MODE
        fprintf(stderr, "FAILED \"%s\"\n", dp->name.c_str());
        fflush(stderr);
#endif
        fprintf(stderr, "%s: Driver %s: execlp: %s\n", indi_tstamp(NULL), dp->name.c_str(), strerror(errno));
        _exit(1); /* parent will notice EOF shortly */
    }

    /* don't need child's side of pipes */
    close(wp[0]);
    close(rp[1]);
    close(ep[1]);

    /* record pid, io channels, init lp and snoop list */
    dp->pid = pid;
    strncpy(dp->host, "localhost", MAXSBUF);
    dp->port    = -1;
    dp->rfd     = rp[0];
    dp->wfd     = wp[1];
    dp->efd     = ep[0];
    dp->lp      = newLilXML();
    dp->nsent   = 0;
    dp->ndev    = 0;
    dp->dev     = (char **)malloc(sizeof(char *));

    /* first message primes driver to report its properties -- dev known
     * if restarting
     */
    mp = new Msg();
    dp->msgq.push_back(mp);
    snprintf(buf, sizeof(buf), "<getProperties version='%g'/>\n", INDIV);
    mp->setFromStr(buf);
    mp->count++;

    if (verbose > 0)
        fprintf(stderr, "%s: Driver %s: pid=%d rfd=%d wfd=%d efd=%d\n", indi_tstamp(NULL), dp->name.c_str(), dp->pid, dp->rfd,
                dp->wfd, dp->efd);
}

/* start the given remote INDI driver connection.
 * exit if trouble.
 */
static void startRemoteDvr(DvrInfo *dp)
{
    char dev[MAXINDIDEVICE] = {0};
    char host[MAXSBUF] = {0};
    char buf[MAXSBUF] = {0};
    int indi_port, sockfd;

    /* extract host and port */
    indi_port = INDIPORT;
    if (sscanf(dp->name.c_str(), "%[^@]@%[^:]:%d", dev, host, &indi_port) < 2)
    {
        // Device missing? Try a different syntax for all devices
        if (sscanf(dp->name.c_str(), "@%[^:]:%d", host, &indi_port) < 1)
        {
            fprintf(stderr, "Bad remote device syntax: %s\n", dp->name.c_str());
            Bye();
        }
    }

    /* connect */
    sockfd = openINDIServer(host, indi_port);

    /* record flag pid, io channels, init lp and snoop list */
    dp->pid = REMOTEDVR;
    strncpy(dp->host, host, MAXSBUF);
    dp->port    = indi_port;
    dp->rfd     = sockfd;
    dp->wfd     = sockfd;
    dp->lp      = newLilXML();
    dp->nsent   = 0;
    dp->ndev    = 1;
    dp->dev     = (char **)malloc(sizeof(char *));

    /* N.B. storing name now is key to limiting outbound traffic to this
     * dev.
     */
    dp->dev[0] = (char *)malloc(MAXINDIDEVICE * sizeof(char));
    strncpy(dp->dev[0], dev, MAXINDIDEVICE - 1);
    dp->dev[0][MAXINDIDEVICE - 1] = '\0';

    /* Sending getProperties with device lets remote server limit its
     * outbound (and our inbound) traffic on this socket to this device.
     */
    Msg *mp = new Msg();
    dp->msgq.push_back(mp);
    if (dev[0])
        sprintf(buf, "<getProperties device='%s' version='%g'/>\n", dp->dev[0], INDIV);
    else
        // This informs downstream server that it is connecting to an upstream server
        // and not a regular client. The difference is in how it treats snooping properties
        // among properties.
        sprintf(buf, "<getProperties device='*' version='%g'/>\n", INDIV);
    mp->setFromStr(buf);
    mp->count++;

    if (verbose > 0)
        fprintf(stderr, "%s: Driver %s: socket=%d\n", indi_tstamp(NULL), dp->name.c_str(), sockfd);
}

/* open a connection to the given host and port or die.
 * return socket fd.
 */
static int openINDIServer(char host[], int indi_port)
{
    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int sockfd;

    /* lookup host address */
    hp = gethostbyname(host);
    if (!hp)
    {
        fprintf(stderr, "gethostbyname(%s): %s\n", host, strerror(errno));
        Bye();
    }

    /* create a socket to the INDI server */
    (void)memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port        = htons(indi_port);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket(%s,%d): %s\n", host, indi_port, strerror(errno));
        Bye();
    }

    /* connect */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "connect(%s,%d): %s\n", host, indi_port, strerror(errno));
        Bye();
    }

    /* ok */
    return (sockfd);
}

/* create the public INDI Driver endpoint lsocket on port.
 * return server socket else exit.
 */
static void indiListen()
{
    struct sockaddr_in serv_socket;
    int sfd;
    int reuse = 1;

    /* make socket endpoint */
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "%s: socket: %s\n", indi_tstamp(NULL), strerror(errno));
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
        fprintf(stderr, "%s: setsockopt: %s\n", indi_tstamp(NULL), strerror(errno));
        Bye();
    }
    if (bind(sfd, (struct sockaddr *)&serv_socket, sizeof(serv_socket)) < 0)
    {
        fprintf(stderr, "%s: bind: %s\n", indi_tstamp(NULL), strerror(errno));
        Bye();
    }

    /* willing to accept connections with a backlog of 5 pending */
    if (listen(sfd, 5) < 0)
    {
        fprintf(stderr, "%s: listen: %s\n", indi_tstamp(NULL), strerror(errno));
        Bye();
    }

    /* ok */
    lsocket = sfd;
    if (verbose > 0)
        fprintf(stderr, "%s: listening to port %d on fd %d\n", indi_tstamp(NULL), port, sfd);
}

/* Attempt to open up FIFO */
static void indiFIFO(void)
{
    if (!fifo)
    {
        return;
    }
    if (fifo->fd != -1)
    {
        close(fifo->fd);
        fifo->fd = -1;
    }

    /* Open up FIFO, if available */
    fifo->fd = open(fifo->name.c_str(), O_RDWR | O_NONBLOCK);

    if (fifo->fd < 0)
    {
        fprintf(stderr, "%s: open(%s): %s.\n", indi_tstamp(NULL), fifo->name.c_str(), strerror(errno));
        Bye();
    }
}

/* service traffic from clients and drivers */
static void indiRun(void)
{
    fd_set rs, ws;
    int maxfd = 0;
    int s;

    /* init with no writers or readers */
    FD_ZERO(&ws);
    FD_ZERO(&rs);

    if (fifo)
    {
        FD_SET(fifo->fd, &rs);
        maxfd = fifo->fd;
    }

    /* always listen for new clients */
    FD_SET(lsocket, &rs);
    if (lsocket > maxfd)
        maxfd = lsocket;

    /* add all client readers and client writers with work to send */
    for (auto cp : clients) 
    {
        FD_SET(cp->s, &rs);
        if (!cp->msgq.empty())
            FD_SET(cp->s, &ws);
        if (cp->s > maxfd)
            maxfd = cp->s;
    }

    /* add all driver readers and driver writers with work to send */
    for (auto dp : drivers)
    {
        FD_SET(dp->rfd, &rs);
        if (dp->rfd > maxfd)
            maxfd = dp->rfd;
        if (dp->pid != REMOTEDVR)
        {
            FD_SET(dp->efd, &rs);
            if (dp->efd > maxfd)
                maxfd = dp->efd;
        }
        if (!dp->msgq.empty())
        {
            FD_SET(dp->wfd, &ws);
            if (dp->wfd > maxfd)
                maxfd = dp->wfd;
        }
    }

    /* wait for action */
    s = select(maxfd + 1, &rs, &ws, NULL, NULL);
    if (s < 0)
    {
        if(errno == EINTR)
            return;
        fprintf(stderr, "%s: select(%d): %s\n", indi_tstamp(NULL), maxfd + 1, strerror(errno));
        Bye();
    }

    /* new command from FIFO? */
    if (s > 0 && fifo && FD_ISSET(fifo->fd, &rs))
    {
        newFIFO();
        s--;
    }

    /* new client? */
    if (s > 0 && FD_ISSET(lsocket, &rs))
    {
        newClient();
        s--;
    }

    /* message to/from client? */
    for (auto cp : clients)
    {
        if (FD_ISSET(cp->s, &rs))
        {
            if (readFromClient(cp) < 0)
                return; /* fds effected */
            s--;
        }
        if (s > 0 && FD_ISSET(cp->s, &ws))
        {
            if (sendClientMsg(cp) < 0)
                return; /* fds effected */
            s--;
        }
    }

    /* message to/from driver? */
    for (auto dp : drivers)
    {
        if (dp->pid != REMOTEDVR && FD_ISSET(dp->efd, &rs))
        {
            if (stderrFromDriver(dp) < 0)
                return; /* fds effected */
            s--;
        }
        if (s > 0 && FD_ISSET(dp->rfd, &rs))
        {
            if (readFromDriver(dp) < 0)
                return; /* fds effected */
            s--;
        }
        if (s > 0 && FD_ISSET(dp->wfd, &ws) && !dp->msgq.empty())
        {
            if (sendDriverMsg(dp) < 0)
                return; /* fds effected */
            s--;
        }
    }
}

int isDeviceInDriver(const char *dev, DvrInfo *dp)
{
    int i = 0;
    for (i = 0; i < dp->ndev; i++)
    {
        if (!strcmp(dev, dp->dev[i]))
            return 1;
    }

    return 0;
}

/* Read commands from FIFO and process them. Start/stop drivers accordingly */
static void newFIFO(void)
{
    //char line[MAXRBUF], tDriver[MAXRBUF], tConfig[MAXRBUF], tDev[MAXRBUF], tSkel[MAXRBUF], envDev[MAXRBUF], envConfig[MAXRBUF], envSkel[MAXR];
    char line[MAXRBUF];
    int startCmd = 0, i = 0, remoteDriver = 0;

    while (i < MAXRBUF)
    {
        if (read(fifo->fd, line + i, 1) <= 0)
        {
            // Reset FIFO now, otherwise select will always return with no data from FIFO.
            indiFIFO();
            return;
        }

        if (line[i] == '\n')
        {
            line[i] = '\0';
            i       = 0;
        }
        else
        {
            i++;
            continue;
        }

        if (verbose)
            fprintf(stderr, "FIFO: %s\n", line);

        char cmd[MAXSBUF], arg[4][1], var[4][MAXSBUF], tDriver[MAXSBUF], tName[MAXSBUF], envConfig[MAXSBUF],
             envSkel[MAXSBUF], envPrefix[MAXSBUF];

        memset(&tDriver[0], 0, sizeof(char) * MAXSBUF);
        memset(&tName[0], 0, sizeof(char) * MAXSBUF);
        memset(&envConfig[0], 0, sizeof(char) * MAXSBUF);
        memset(&envSkel[0], 0, sizeof(char) * MAXSBUF);
        memset(&envPrefix[0], 0, sizeof(char) * MAXSBUF);

        int n = 0;

        // If remote driver
        if (strstr(line, "@"))
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
            remoteDriver = 1;
        }
        // If local driver
        else
        {
            n = sscanf(line, "%s %s -%1c \"%512[^\"]\" -%1c \"%512[^\"]\" -%1c \"%512[^\"]\" -%1c \"%512[^\"]\"", cmd,
                       tDriver, arg[0], var[0], arg[1], var[1], arg[2], var[2], arg[3], var[3]);
            remoteDriver = 0;
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
                    fprintf(stderr, "With name: %s\n", tName);
            }
            else if (arg[j][0] == 'c')
            {
                strncpy(envConfig, var[j], MAXSBUF - 1);
                envConfig[MAXSBUF - 1] = '\0';

                if (verbose)
                    fprintf(stderr, "With config: %s\n", envConfig);
            }
            else if (arg[j][0] == 's')
            {
                strncpy(envSkel, var[j], MAXSBUF - 1);
                envSkel[MAXSBUF - 1] = '\0';

                if (verbose)
                    fprintf(stderr, "With skeketon: %s\n", envSkel);
            }
            else if (arg[j][0] == 'p')
            {
                strncpy(envPrefix, var[j], MAXSBUF - 1);
                envPrefix[MAXSBUF - 1] = '\0';

                if (verbose)
                    fprintf(stderr, "With prefix: %s\n", envPrefix);
            }
        }

        if (!strcmp(cmd, "start"))
            startCmd = 1;
        else
            startCmd = 0;

        if (startCmd)
        {
            if (verbose)
                fprintf(stderr, "FIFO: Starting driver %s\n", tDriver);
            DvrInfo *dp = new DvrInfo();
            dp->name = tDriver;

            if (remoteDriver == 0)
            {
                //strncpy(dp->dev, tName, MAXINDIDEVICE);
                strncpy(dp->envDev, tName, MAXSBUF);
                strncpy(dp->envConfig, envConfig, MAXSBUF);
                strncpy(dp->envSkel, envSkel, MAXSBUF);
                strncpy(dp->envPrefix, envPrefix, MAXSBUF);
                startDvr(dp);
            }
            else
                startRemoteDvr(dp);
        }
        else
        {
            for (auto dp : drivers)
            {
                fprintf(stderr, "dp->name: %s - tDriver: %s\n", dp->name.c_str(), tDriver);
                if (dp->name == tDriver)
                {
                    fprintf(stderr, "name: %s - dp->dev[0]: %s\n", tName, dp->dev[0]);

                    /* If device name is given, check against it before shutting down */
                    //if (tName[0] && strcmp(dp->dev[0], tName))
                    if (tName[0] && isDeviceInDriver(tName, dp) == 0)
                        continue;
                    if (verbose)
                        fprintf(stderr, "FIFO: Shutting down driver: %s\n", tDriver);

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

                    shutdownDvr(dp, 0);
                    break;
                }
            }
        }
    }
}

/* prepare for new client arriving on lsocket.
 * exit if trouble.
 */
static void newClient()
{
    ClInfo * cp = new ClInfo();
    int s;

    /* assign new socket */
    s = newClSocket();

    /* rig up new clinfo entry */
    cp->s      = s;
    cp->lp     = newLilXML();
    cp->nsent  = 0;

    if (verbose > 0)
    {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        getpeername(s, (struct sockaddr *)&addr, &len);
        fprintf(stderr, "%s: Client %d: new arrival from %s:%d - welcome!\n", indi_tstamp(NULL), cp->s,
                inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    }
#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "CLIENTS %d\n", clients.size());
    fflush(stderr);
#endif
}

/* read more from the given client, send to each appropriate driver when see
 * xml closure. also send all newXXX() to all other interested clients.
 * return -1 if had to shut down anything, else 0.
 */
static int readFromClient(ClInfo *cp)
{
    char buf[MAXRBUF];
    int shutany = 0;
    ssize_t i, nr;

    /* read client */
    nr = read(cp->s, buf, sizeof(buf));
    if (nr <= 0)
    {
        if (nr < 0)
            fprintf(stderr, "%s: Client %d: read: %s\n", indi_tstamp(NULL), cp->s, strerror(errno));
        else if (verbose > 0)
            fprintf(stderr, "%s: Client %d: read EOF\n", indi_tstamp(NULL), cp->s);
        shutdownClient(cp);
        return (-1);
    }

    /* process XML, sending when find closure */
    for (i = 0; i < nr; i++)
    {
        char err[1024];
        XMLEle *root = readXMLEle(cp->lp, buf[i], err);
        if (root)
        {
            char *roottag    = tagXMLEle(root);
            const char *dev  = findXMLAttValu(root, "device");
            const char *name = findXMLAttValu(root, "name");
            int isblob       = !strcmp(tagXMLEle(root), "setBLOBVector");

            if (verbose > 2)
            {
                fprintf(stderr, "%s: Client %d: read ", indi_tstamp(NULL), cp->s);
                traceMsg(root);
            }
            else if (verbose > 1)
            {
                fprintf(stderr, "%s: Client %d: read <%s device='%s' name='%s'>\n", indi_tstamp(NULL), cp->s,
                        tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"));
            }

            /* snag interested properties.
            * N.B. don't open to alldevs if seen specific dev already, else
            *   remote client connections start returning too much.
            */
            if (dev[0])
            {
                // Signature for CHAINED SERVER
                // Not a regular client.
                if (dev[0] == '*' && !cp->props.size())
                    cp->allprops = 2;
                else
                    addClDevice(cp, dev, name, isblob);
            }
            else if (!strcmp(roottag, "getProperties") && !cp->props.size() && cp->allprops != 2)
                cp->allprops = 1;

            /* snag enableBLOB -- send to remote drivers too */
            if (!strcmp(roottag, "enableBLOB"))
                crackBLOBHandling(dev, name, pcdataXMLEle(root), cp);

            /* build a new message -- set content iff anyone cares */
            Msg* mp = new Msg();

            /* send message to driver(s) responsible for dev */
            q2RDrivers(dev, mp, root);

            /* JM 2016-05-18: Upstream client can be a chained INDI server. If any driver locally is snooping
            * on any remote drivers, we should catch it and forward it to the responsible snooping driver. */
            /* send to snooping drivers. */
            // JM 2016-05-26: Only forward setXXX messages
            if (!strncmp(roottag, "set", 3))
                q2SDrivers(NULL, isblob, dev, name, mp, root);

            /* echo new* commands back to other clients */
            if (!strncmp(roottag, "new", 3))
            {
                if (q2Clients(cp, isblob, dev, name, mp, root) < 0)
                    shutany++;
            }

            /* set message content if anyone cares else forget it */
            if (mp->count > 0)
                mp->setFromXMLEle(root);
            else
                delete mp;
            delXMLEle(root);
        }
        else if (err[0])
        {
            char *ts = indi_tstamp(NULL);
            fprintf(stderr, "%s: Client %d: XML error: %s\n", ts, cp->s, err);
            fprintf(stderr, "%s: Client %d: XML read: %.*s\n", ts, cp->s, (int)nr, buf);
            shutdownClient(cp);
            return (-1);
        }
    }

    return (shutany ? -1 : 0);
}

/* read more from the given driver, send to each interested client when see
 * xml closure. if driver dies, try restarting.
 * return 0 if ok else -1 if had to shut down anything.
 */
static int readFromDriver(DvrInfo *dp)
{
    char buf[MAXRBUF];
    int shutany = 0;
    ssize_t nr;
    char err[1024];
    XMLEle **nodes;
    XMLEle *root;
    int inode = 0;

    /* read driver */
    nr = read(dp->rfd, buf, sizeof(buf));
    if (nr <= 0)
    {
        if (nr < 0)
            fprintf(stderr, "%s: Driver %s: stdin %s\n", indi_tstamp(NULL), dp->name.c_str(), strerror(errno));
        else
            fprintf(stderr, "%s: Driver %s: stdin EOF\n", indi_tstamp(NULL), dp->name.c_str());

        shutdownDvr(dp, 1);
        return (-1);
    }

    /* process XML chunk */
    nodes = parseXMLChunk(dp->lp, buf, nr, err);

    if (!nodes)
    {
        if (err[0])
        {
            char *ts = indi_tstamp(NULL);
            fprintf(stderr, "%s: Driver %s: XML error: %s\n", ts, dp->name.c_str(), err);
            fprintf(stderr, "%s: Driver %s: XML read: %.*s\n", ts, dp->name.c_str(), (int)nr, buf);
            shutdownDvr(dp, 1);
            return (-1);
        }
        return -1;
    }

    root = nodes[inode];
    while (root)
    {
        char *roottag    = tagXMLEle(root);
        const char *dev  = findXMLAttValu(root, "device");
        const char *name = findXMLAttValu(root, "name");
        int isblob       = !strcmp(tagXMLEle(root), "setBLOBVector");

        if (verbose > 2)
        {
            fprintf(stderr, "%s: Driver %s: read ", indi_tstamp(0), dp->name.c_str());
            traceMsg(root);
        }
        else if (verbose > 1)
        {
            fprintf(stderr, "%s: Driver %s: read <%s device='%s' name='%s'>\n", indi_tstamp(NULL), dp->name.c_str(),
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"));
        }

        /* that's all if driver is just registering a snoop */
        /* JM 2016-05-18: Send getProperties to upstream chained servers as well.*/
        if (!strcmp(roottag, "getProperties"))
        {
            addSDevice(dp, dev, name);
            Msg *mp = new Msg();
            /* send to interested chained servers upstream */
            if (q2Servers(dp, mp, root) < 0)
                shutany++;
            /* Send to snooped drivers if they exist so that they can echo back the snooped propertly immediately */
            q2RDrivers(dev, mp, root);

            if (mp->count > 0)
                mp->setFromXMLEle(root);
            else
                delete(mp);
            delXMLEle(root);
            inode++;
            root = nodes[inode];
            continue;
        }

        /* that's all if driver desires to snoop BLOBs from other drivers */
        if (!strcmp(roottag, "enableBLOB"))
        {
            Property *sp = findSDevice(dp, dev, name);
            if (sp)
                crackBLOB(pcdataXMLEle(root), &sp->blob);
            delXMLEle(root);
            inode++;
            root = nodes[inode];
            continue;
        }

        /* Found a new device? Let's add it to driver info */
        if (dev[0] && isDeviceInDriver(dev, dp) == 0)
        {
            dp->dev           = (char **)realloc(dp->dev, (dp->ndev + 1) * sizeof(char *));
            dp->dev[dp->ndev] = (char *)malloc(MAXINDIDEVICE * sizeof(char));

            strncpy(dp->dev[dp->ndev], dev, MAXINDIDEVICE - 1);
            dp->dev[dp->ndev][MAXINDIDEVICE - 1] = '\0';

#ifdef OSX_EMBEDED_MODE
            if (!dp->ndev)
                fprintf(stderr, "STARTED \"%s\"\n", dp->name.c_str());
            fflush(stderr);
#endif

            dp->ndev++;
        }

        /* log messages if any and wanted */
        if (ldir)
            logDMsg(root, dev);

        /* build a new message -- set content iff anyone cares */
        Msg * mp = new Msg();

        /* send to interested clients */
        if (q2Clients(NULL, isblob, dev, name, mp, root) < 0)
            shutany++;

        /* send to snooping drivers */
        q2SDrivers(dp, isblob, dev, name, mp, root);

        /* set message content if anyone cares else forget it */
        if (mp->count > 0)
            mp->setFromXMLEle(root);
        else
            delete mp;
        delXMLEle(root);
        inode++;
        root = nodes[inode];
    }

    free(nodes);

    return (shutany ? -1 : 0);
}

/* read more from the given driver stderr, add prefix and send to our stderr.
 * return 0 if ok else -1 if had to restart.
 */
static int stderrFromDriver(DvrInfo *dp)
{
    static char exbuf[MAXRBUF];
    static int nexbuf;
    ssize_t i, nr;

    /* read more */
    nr = read(dp->efd, exbuf + nexbuf, sizeof(exbuf) - nexbuf);
    if (nr <= 0)
    {
        if (nr < 0)
            fprintf(stderr, "%s: Driver %s: stderr %s\n", indi_tstamp(NULL), dp->name.c_str(), strerror(errno));
        else
            fprintf(stderr, "%s: Driver %s: stderr EOF\n", indi_tstamp(NULL), dp->name.c_str());
        shutdownDvr(dp, 1);
        return (-1);
    }
    nexbuf += nr;

    /* prefix each whole line to our stderr, save extra for next time */
    for (i = 0; i < nexbuf; i++)
    {
        if (exbuf[i] == '\n')
        {
            fprintf(stderr, "%s: Driver %s: %.*s\n", indi_tstamp(NULL), dp->name.c_str(), (int)i, exbuf);
            i++;                               /* count including nl */
            nexbuf -= i;                       /* remove from nexbuf */
            memmove(exbuf, exbuf + i, nexbuf); /* slide remaining to front */
            i = -1;                            /* restart for loop scan */
        }
    }

    return (0);
}

/* close down the given client */
static void shutdownClient(ClInfo *cp)
{
    /* close connection */
    shutdown(cp->s, SHUT_RDWR);
    close(cp->s);

    /* free memory */
    delLilXML(cp->lp);

    if (verbose > 0)
        fprintf(stderr, "%s: Client %d: shut down complete - bye!\n", indi_tstamp(NULL), cp->s);

    delete(cp);

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "CLIENTS %d\n", clients.size());
    fflush(stderr);
#endif
}

/* close down the given driver and restart */
static void shutdownDvr(DvrInfo *dp, int restart)
{
    int i = 0;

    // Tell client driver is dead.
    for (i = 0; i < dp->ndev; i++)
    {
        /* Inform clients that this driver is dead */
        XMLEle *root = addXMLEle(NULL, "delProperty");
        addXMLAtt(root, "device", dp->dev[i]);

        prXMLEle(stderr, root, 0);
        Msg *mp = new Msg();

        q2Clients(NULL, 0, dp->dev[i], NULL, mp, root);
        if (mp->count > 0)
            mp->setFromXMLEle(root);
        else
            delete mp;
        delXMLEle(root);
    }

    /* make sure it's dead, reclaim resources */
    if (dp->pid == REMOTEDVR)
    {
        /* socket connection */
        shutdown(dp->wfd, SHUT_RDWR);
        close(dp->wfd); /* same as rfd */
    }
    else
    {
        /* local pipe connection */
        kill(dp->pid, SIGKILL); /* we've insured there are no zombies */
        close(dp->wfd);
        close(dp->rfd);
        close(dp->efd);
    }

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "STOPPED \"%s\"\n", dp->name.c_str());
    fflush(stderr);
#endif

    /* free memory */
    free(dp->dev);
    delLilXML(dp->lp);

    /* ok now to recycle */
    dp->ndev   = 0;

    /* decrement and possibly free any unsent messages for this client */
    dp->clearMsgQueue();

    if (restart)
    {
        if (dp->restarts >= maxrestarts)
        {
            fprintf(stderr, "%s: Driver %s: Terminated after #%d restarts.\n", indi_tstamp(NULL), dp->name.c_str(),
                    dp->restarts);
            // If we're not in FIFO mode and we do not have any more drivers, shutdown the server
            if ((!fifo) && (drivers.size() == 0))
                Bye();
            delete(dp);
        }
        else
        {
            fprintf(stderr, "%s: Driver %s: restart #%d\n", indi_tstamp(NULL), dp->name.c_str(), ++dp->restarts);
            startDvr(dp);
        }
    } else {
        delete(dp);
    }
}

/* put Msg mp on queue of each driver responsible for dev, or all drivers
 * if dev not specified.
 */
static void q2RDrivers(const char *dev, Msg *mp, XMLEle *root)
{
    char *roottag = tagXMLEle(root);

    char lastRemoteHost[MAXSBUF];
    int lastRemotePort = -1;

    /* queue message to each interested driver.
     * N.B. don't send generic getProps to more than one remote driver,
     *   otherwise they all fan out and we get multiple responses back.
     */
    for (auto dp : drivers)
    {
        int isRemote = (dp->pid == REMOTEDVR);

        /* driver known to not support this dev */
        if (dev[0] && dev[0] != '*' && isDeviceInDriver(dev, dp) == 0)
            continue;

        /* Only send message to each *unique* remote driver at a particular host:port
         * Since it will be propogated to all other devices there */
        if (!dev[0] && isRemote && !strcmp(lastRemoteHost, dp->host) && lastRemotePort == dp->port)
            continue;

        /* JM 2016-10-30: Only send enableBLOB to remote drivers */
        if (isRemote == 0 && !strcmp(roottag, "enableBLOB"))
            continue;

        /* Retain last remote driver data so that we do not send the same info again to a driver
         * residing on the same host:port */
        if (isRemote)
        {
            strncpy(lastRemoteHost, dp->host, MAXSBUF);
            lastRemotePort = dp->port;
        }

        /* ok: queue message to this driver */
        dp->pushMsg(mp);
        if (verbose > 1)
        {
            fprintf(stderr, "%s: Driver %s: queuing responsible for <%s device='%s' name='%s'>\n", indi_tstamp(NULL),
                    dp->name.c_str(), tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"));
        }
    }
}

/* put Msg mp on queue of each driver snooping dev/name.
 * if BLOB always honor current mode.
 */
static void q2SDrivers(DvrInfo *me, int isblob, const char *dev, const char *name, Msg *mp, XMLEle *root)
{
    for (auto dp : drivers)
    {
        Property *sp = findSDevice(dp, dev, name);

        /* nothing for dp if not snooping for dev/name or wrong BLOB mode */
        if (!sp)
            continue;
        if ((isblob && sp->blob == B_NEVER) || (!isblob && sp->blob == B_ONLY))
            continue;
        if (me && me->pid == REMOTEDVR && dp->pid == REMOTEDVR)
        {
            // Do not send snoop data to remote drivers at the same host
            // since they will manage their own snoops remotely
            if (!strcmp(me->host, dp->host) && me->port == dp->port)
                continue;
        }

        /* ok: queue message to this device */
        dp->pushMsg(mp);
        if (verbose > 1)
        {
            fprintf(stderr, "%s: Driver %s: queuing snooped <%s device='%s' name='%s'>\n", indi_tstamp(NULL), dp->name.c_str(),
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"));
        }
    }
}

/* add dev/name to dp's snooping list.
 * init with blob mode set to B_NEVER.
 */
static void addSDevice(DvrInfo *dp, const char *dev, const char *name)
{
    Property *sp;
    char *ip;

    /* no dups */
    sp = findSDevice(dp, dev, name);
    if (sp)
        return;

    /* add dev to sdevs list */
    sp         = new Property();
    dp->sprops.push_back(sp);

    ip = sp->dev;
    strncpy(ip, dev, MAXINDIDEVICE - 1);
    ip[MAXINDIDEVICE - 1] = '\0';

    ip = sp->name;
    strncpy(ip, name, MAXINDINAME - 1);
    ip[MAXINDINAME - 1] = '\0';

    sp->blob = B_NEVER;

    if (verbose)
        fprintf(stderr, "%s: Driver %s: snooping on %s.%s\n", indi_tstamp(NULL), dp->name.c_str(), dev, name);
}

/* return Property if dp is snooping dev/name, else NULL.
 */
static Property *findSDevice(DvrInfo *dp, const char *dev, const char *name)
{
    for(auto sp : dp->sprops) 
    {
        if (!strcmp(sp->dev, dev) && (!sp->name[0] || !strcmp(sp->name, name)))
            return (sp);
    }

    return (NULL);
}

/* put Msg mp on queue of each client interested in dev/name, except notme.
 * if BLOB always honor current mode.
 * return -1 if had to shut down any clients, else 0.
 */
static int q2Clients(ClInfo *notme, int isblob, const char *dev, const char *name, Msg *mp, XMLEle *root)
{
    int shutany = 0;

    /* queue message to each interested client */
    for (auto cp : clients)
    {
        /* cp in use? notme? want this dev/name? blob? */
        if (cp == notme)
            continue;
        if (findClDevice(cp, dev, name) < 0)
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
                    if (!strcmp(pp->dev, dev) && (!strcmp(pp->name, name)))
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
                    fprintf(stderr, "%s: Client %d: %ld bytes behind. Dropping stream BLOB...\n", indi_tstamp(NULL),
                            cp->s, ql);
                continue;
            }
        }
        if (ql > maxqsiz)
        {
            if (verbose)
                fprintf(stderr, "%s: Client %d: %ld bytes behind, shutting down\n", indi_tstamp(NULL), cp->s, ql);
            shutdownClient(cp);
            shutany++;
            continue;
        }

        /* ok: queue message to this client */
        cp->pushMsg(mp);
        if (verbose > 1)
            fprintf(stderr, "%s: Client %d: queuing <%s device='%s' name='%s'>\n", indi_tstamp(NULL), cp->s,
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"));
    }

    return (shutany ? -1 : 0);
}

/* put Msg mp on queue of each chained server client, except notme.
  * return -1 if had to shut down any clients, else 0.
 */
static int q2Servers(DvrInfo *me, Msg *mp, XMLEle *root)
{
    int shutany = 0, devFound = 0;

    /* queue message to each interested client */
    for (auto cp : clients)
    {
        // Only send the message to the upstream server that is connected specfically to the device in driver dp
        switch (cp->allprops)
        {
            // 0 --> not all props are requested. Check for specific combination
            case 0:
                for (auto pp : cp->props)
                {
                    int j        = 0;
                    for (j = 0; j < me->ndev; j++)
                    {
                        if (!strcmp(pp->dev, me->dev[j]))
                            break;
                    }

                    if (j != me->ndev)
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
        if (ql > maxqsiz)
        {
            if (verbose)
                fprintf(stderr, "%s: Client %d: %ld bytes behind, shutting down\n", indi_tstamp(NULL), cp->s, ql);
            shutdownClient(cp);
            shutany++;
            continue;
        }

        /* ok: queue message to this client */
        cp->pushMsg(mp);
        if (verbose > 1)
            fprintf(stderr, "%s: Client %d: queuing <%s device='%s' name='%s'>\n", indi_tstamp(NULL), cp->s,
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"));
    }

    return (shutany ? -1 : 0);
}

/* write the next chunk of the current message in the queue to the given
 * client. pop message from queue when complete and free the message if we are
 * the last one to use it. shut down this client if trouble.
 * N.B. we assume we will never be called with cp->msgq empty.
 * return 0 if ok else -1 if had to shut down.
 */
static int sendClientMsg(ClInfo *cp)
{
    ssize_t nsend, nw;
    Msg *mp;

    /* get current message */
    mp = cp->headMsg();

    /* send next chunk, never more than MAXWSIZ to reduce blocking */
    nsend = mp->cl - cp->nsent;
    if (nsend > MAXWSIZ)
        nsend = MAXWSIZ;
    nw = write(cp->s, &mp->cp[cp->nsent], nsend);

    /* shut down if trouble */
    if (nw <= 0)
    {
        if (nw == 0)
            fprintf(stderr, "%s: Client %d: write returned 0\n", indi_tstamp(NULL), cp->s);
        else
            fprintf(stderr, "%s: Client %d: write: %s\n", indi_tstamp(NULL), cp->s, strerror(errno));
        shutdownClient(cp);
        return (-1);
    }

    /* trace */
    if (verbose > 2)
    {
        fprintf(stderr, "%s: Client %d: sending msg copy %d nq %ld:\n%.*s\n", indi_tstamp(NULL), cp->s, mp->count,
                cp->msgq.size(), (int)nw, &mp->cp[cp->nsent]);
    }
    else if (verbose > 1)
    {
        fprintf(stderr, "%s: Client %d: sending %.50s\n", indi_tstamp(NULL), cp->s, &mp->cp[cp->nsent]);
    }

    /* update amount sent. when complete: free message if we are the last
     * to use it and pop from our queue.
     */
    cp->nsent += nw;
    if (cp->nsent == mp->cl)
        cp->consumeHeadMsg();

    return (0);
}

/* write the next chunk of the current message in the queue to the given
 * driver. pop message from queue when complete and free the message if we are
 * the last one to use it. restart this driver if touble.
 * N.B. we assume we will never be called with dp->msgq empty.
 * return 0 if ok else -1 if had to shut down.
 */
static int sendDriverMsg(DvrInfo *dp)
{
    ssize_t nsend, nw;
    Msg *mp;

    /* get current message */
    mp = dp->headMsg();

    /* send next chunk, never more than MAXWSIZ to reduce blocking */
    nsend = mp->cl - dp->nsent;
    if (nsend > MAXWSIZ)
        nsend = MAXWSIZ;
    nw = write(dp->wfd, &mp->cp[dp->nsent], nsend);

    /* restart if trouble */
    if (nw <= 0)
    {
        if (nw == 0)
            fprintf(stderr, "%s: Driver %s: write returned 0\n", indi_tstamp(NULL), dp->name.c_str());
        else
            fprintf(stderr, "%s: Driver %s: write: %s\n", indi_tstamp(NULL), dp->name.c_str(), strerror(errno));
        shutdownDvr(dp, 1);
        return (-1);
    }

    /* trace */
    if (verbose > 2)
    {
        fprintf(stderr, "%s: Driver %s: sending msg copy %d nq %ld:\n%.*s\n", indi_tstamp(NULL), dp->name.c_str(), mp->count,
                dp->msgq.size(), (int)nw, &mp->cp[dp->nsent]);
    }
    else if (verbose > 1)
    {
        fprintf(stderr, "%s: Driver %s: sending %.50s\n", indi_tstamp(NULL), dp->name.c_str(), &mp->cp[dp->nsent]);
    }

    /* update amount sent. when complete: free message if we are the last
     * to use it and pop from our queue.
     */
    dp->nsent += nw;
    if (dp->nsent == mp->cl)
        dp->consumeHeadMsg();

    return (0);
}

/* return 0 if cp may be interested in dev/name else -1
 */
static int findClDevice(ClInfo *cp, const char *dev, const char *name)
{
    if (cp->allprops >= 1 || !dev[0])
        return (0);
    for (auto pp : cp->props)
    {
        if (!strcmp(pp->dev, dev) && (!pp->name[0] || !strcmp(pp->name, name)))
            return (0);
    }
    return (-1);
}

/* add the given device and property to the devs[] list of client if new.
 */
static void addClDevice(ClInfo *cp, const char *dev, const char *name, int isblob)
{
    if (isblob)
    {
        for (auto pp : cp->props)
        {
            if (!strcmp(pp->dev, dev) && (name == NULL || !strcmp(pp->name, name)))
                return;
        }
    }
    /* no dups */
    else if (!findClDevice(cp, dev, name))
        return;

    /* add */
    Property *pp = new Property();
    cp->props.push_back(pp);

    /*ip = pp->dev;
    strncpy (ip, dev, MAXINDIDEVICE-1);
    ip[MAXINDIDEVICE-1] = '\0';

    ip = pp->name;
    strncpy (ip, name, MAXINDINAME-1);
        ip[MAXINDINAME-1] = '\0';*/

    strncpy(pp->dev, dev, MAXINDIDEVICE);
    strncpy(pp->name, name, MAXINDINAME);
    pp->blob = B_NEVER;
}

/* block to accept a new client arriving on lsocket.
 * return private nonblocking socket or exit.
 */
static int newClSocket()
{
    struct sockaddr_in cli_socket;
    socklen_t cli_len;
    int cli_fd;

    /* get a private connection to new client */
    cli_len = sizeof(cli_socket);
    cli_fd  = accept(lsocket, (struct sockaddr *)&cli_socket, &cli_len);
    if (cli_fd < 0)
    {
        fprintf(stderr, "accept: %s\n", strerror(errno));
        Bye();
    }

    /* ok */
    return (cli_fd);
}

/* convert the string value of enableBLOB to our B_ state value.
 * no change if unrecognized
 */
static void crackBLOB(const char *enableBLOB, BLOBHandling *bp)
{
    if (!strcmp(enableBLOB, "Also"))
        *bp = B_ALSO;
    else if (!strcmp(enableBLOB, "Only"))
        *bp = B_ONLY;
    else if (!strcmp(enableBLOB, "Never"))
        *bp = B_NEVER;
}

/* Update the client property BLOB handling policy */
static void crackBLOBHandling(const char *dev, const char *name, const char *enableBLOB, ClInfo *cp)
{
    /* If we have EnableBLOB with property name, we add it to Client device list */
    if (name[0])
        addClDevice(cp, dev, name, 1);
    else
        /* Otherwise, we set the whole client blob handling to what's passed (enableBLOB) */
        crackBLOB(enableBLOB, &cp->blob);

    /* If whole client blob handling policy was updated, we need to pass that also to all children
       and if the request was for a specific property, then we apply the policy to it */
    for (auto pp : cp->props)
    {
        if (!name[0])
            crackBLOB(enableBLOB, &pp->blob);
        else if (!strcmp(pp->dev, dev) && (!strcmp(pp->name, name)))
        {
            crackBLOB(enableBLOB, &pp->blob);
            return;
        }
    }
}

/* print key attributes and values of the given xml to stderr.
 */
static void traceMsg(XMLEle *root)
{
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

DvrInfo::~DvrInfo() {
    drivers.erase(this);
    for(auto prop : sprops) {
        delete prop;
    }
}

ClInfo::ClInfo() {
    clients.insert(this);
}

ClInfo::~ClInfo() {
    for(auto prop : props) {
        delete prop;
    }
    
    /* decrement and possibly free any unsent messages for this client */
    for(auto mp : msgq)
        if (--mp->count == 0)
            delete(mp);
    
    clients.erase(this);
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

void Msg::setFromStr(const char * str) {
    alloc(strlen(str));
    memcpy(cp, str, cl);
}

void Msg::setFromXMLEle(XMLEle *root)
{
    /* want cl to only count content, but need room for final \0 */
    alloc(sprlXMLEle(root, 0));
    sprXMLEle(cp, root, 0);

    /* Drop the last zero */
    cl--;
}

MsgQueue::MsgQueue(): nsent(0) {}

MsgQueue::~MsgQueue() {
    clearMsgQueue();
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
}

void MsgQueue::pushMsg(Msg * mp) {
    mp->count++;
    msgq.push_back(mp);
}

void MsgQueue::clearMsgQueue() {
    for(auto mp: msgq) {
        unrefMsg(mp);
    }
    nsent = 0;
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