/* connect to an INDI server and list all devices
 * exit status: 0 at least one device found, 1 no devices found, 2 real trouble.
 */

#include "indiapi.h"
#include "lilxml.h"

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

/* table of INDI definition elements we look for */
typedef struct
{
    char *vec; /* vector name */
} INDIDef;
static INDIDef defs[] = {
    { "defTextVector" },
    { "defNumberVector" },
    { "defSwitchVector" },
    { "defLightVector" },
    { "defBLOBVector" },
};
static int ndefs = sizeof(defs) / sizeof(defs[0]);

/* simple device list to track unique devices */
typedef struct
{
    char **devices;
    int count;
} DeviceList;
static DeviceList devlist = { NULL, 0 };

static void usage(void);
static void addDevice(const char *dev);
static int hasDevice(const char *dev);
static int matchPattern(const char *str, const char *pattern);
static void openINDIServer(void);
static void getprops(void);
static void listenINDI(void);
static void onAlarm(int dummy);
static int readServerChar(void);
static void findDevices(XMLEle *root);

static char *me;                      /* our name for usage() message */
static char host_def[] = "localhost"; /* default host name */
static char *host      = host_def;    /* working host name */
#define INDIPORT 7624                 /* default port */
static int port = INDIPORT;           /* working port number */
#define TIMEOUT 2                     /* default timeout, secs */
static int timeout = TIMEOUT;         /* working timeout, secs */
static int verbose;                   /* report extra info */
static LilXML *lillp;                 /* XML parser context */
#define WILDCARD '*'                  /* wildcard character */
static int directfd = -1;             /* direct filedes to server, if >= 0 */
static FILE *svrwfp;                  /* FILE * to talk to server */
static FILE *svrrfp;                  /* FILE * to read from server */
static char *devpattern = NULL;       /* device pattern to match */

int main(int ac, char *av[])
{
    /* save our name */
    me = av[0];

    /* crack args */
    while (--ac && **++av == '-')
    {
        char *s = *av;
        while (*++s)
        {
            switch (*s)
            {
                case 'd':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-d requires open fileno\n");
                        usage();
                    }
                    directfd = atoi(*++av);
                    ac--;
                    break;
                case 'h':
                    if (directfd >= 0)
                    {
                        fprintf(stderr, "Can not combine -d and -h\n");
                        usage();
                    }
                    if (ac < 2)
                    {
                        fprintf(stderr, "-h requires host name\n");
                        usage();
                    }
                    host = *++av;
                    ac--;
                    break;
                case 'p':
                    if (directfd >= 0)
                    {
                        fprintf(stderr, "Can not combine -d and -p\n");
                        usage();
                    }
                    if (ac < 2)
                    {
                        fprintf(stderr, "-p requires tcp port number\n");
                        usage();
                    }
                    port = atoi(*++av);
                    ac--;
                    break;
                case 't':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-t requires timeout\n");
                        usage();
                    }
                    timeout = atoi(*++av);
                    ac--;
                    break;
                case 'v': /* verbose */
                    verbose++;
                    break;
                default:
                    fprintf(stderr, "Unknown flag: %c\n", *s);
                    usage();
            }
        }
    }

    /* now ac args starting with av[0] */
    if (ac > 0)
        devpattern = *av; /* optional device pattern */

    /* open connection */
    if (directfd >= 0)
    {
        svrwfp = fdopen(directfd, "w");
        svrrfp = fdopen(directfd, "r");
        if (!svrwfp || !svrrfp)
        {
            fprintf(stderr, "Direct fd %d: %s\n", directfd, strerror(errno));
            exit(2);
        }
        setbuf(svrrfp, NULL); /* don't absorb next guy's stuff */
        if (verbose)
            fprintf(stderr, "Using direct fd %d\n", directfd);
    }
    else
    {
        openINDIServer();
        if (verbose)
            fprintf(stderr, "Connected to %s on port %d\n", host, port);
    }

    /* build a parser context for cracking XML responses */
    lillp = newLilXML();

    /* issue getProperties */
    getprops();

    /* listen for responses, looking for devices or timeout */
    listenINDI();

    /* print all found devices */
    for (int i = 0; i < devlist.count; i++)
    {
        printf("%s\n", devlist.devices[i]);
    }

    /* clean up */
    for (int i = 0; i < devlist.count; i++)
        free(devlist.devices[i]);
    free(devlist.devices);
    delLilXML(lillp);

    return (devlist.count > 0 ? 0 : 1);
}

static void usage()
{
    fprintf(stderr, "Purpose: list devices from an INDI server\n");
    fprintf(stderr, "%s\n", GIT_TAG_STRING);
    fprintf(stderr, "Usage: %s [options] [device_pattern]\n", me);
    fprintf(stderr, "  device_pattern may contain \"*\" to match all (beware shell metacharacters).\n");
    fprintf(stderr, "  Lists all devices if no pattern specified.\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d f  : use file descriptor f already open to server\n");
    fprintf(stderr, "  -h h  : alternate host, default is %s\n", host_def);
    fprintf(stderr, "  -p p  : alternate port, default is %d\n", INDIPORT);
    fprintf(stderr, "  -t t  : max time to wait, default is %d secs\n", TIMEOUT);
    fprintf(stderr, "  -v    : verbose (cumulative)\n");
    fprintf(stderr, "Exit status:\n");
    fprintf(stderr, "  0: found at least one device\n");
    fprintf(stderr, "  1: no devices found\n");
    fprintf(stderr, "  2: real trouble, try repeating with -v\n");

    exit(2);
}

/* add a device to the list if not already present and matches pattern */
static void addDevice(const char *dev)
{
    /* check if matches pattern */
    if (devpattern && !matchPattern(dev, devpattern))
        return;

    /* check if already in list */
    if (hasDevice(dev))
        return;

    /* add to list */
    devlist.devices = (char **)realloc(devlist.devices, (devlist.count + 1) * sizeof(char *));
    devlist.devices[devlist.count] = strdup(dev);
    devlist.count++;

    if (verbose)
        fprintf(stderr, "Found device: %s\n", dev);
}

/* check if device is already in list */
static int hasDevice(const char *dev)
{
    for (int i = 0; i < devlist.count; i++)
    {
        if (strcmp(devlist.devices[i], dev) == 0)
            return 1;
    }
    return 0;
}

/* simple wildcard matching: supports * only */
static int matchPattern(const char *str, const char *pattern)
{
    /* if pattern is just *, match everything */
    if (strcmp(pattern, "*") == 0)
        return 1;

    /* check if pattern has wildcard */
    const char *star = strchr(pattern, WILDCARD);
    if (!star)
    {
        /* no wildcard, exact match */
        return strcmp(str, pattern) == 0;
    }

    /* wildcard present */
    int prefix_len = star - pattern;
    
    /* check prefix */
    if (strncmp(str, pattern, prefix_len) != 0)
        return 0;

    /* if wildcard is at the end, we're done */
    if (star[1] == '\0')
        return 1;

    /* check suffix */
    const char *suffix = star + 1;
    int suffix_len = strlen(suffix);
    int str_len = strlen(str);
    
    if (str_len < prefix_len + suffix_len)
        return 0;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/* open a connection to the given host and port.
 * set svrwfp and svrrfp or die.
 */
static void openINDIServer(void)
{
    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int sockfd;

    /* lookup host address */
    hp = gethostbyname(host);
    if (!hp)
    {
        herror("gethostbyname");
        exit(2);
    }

    /* create a socket to the INDI server */
    (void)memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port        = htons(port);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(2);
    }

    /* connect */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        exit(2);
    }

    /* prepare for line-oriented i/o with client */
    svrwfp = fdopen(sockfd, "w");
    svrrfp = fdopen(sockfd, "r");
}

/* issue getProperties to svrwfp */
static void getprops()
{
    fprintf(svrwfp, "<getProperties version='%g'/>\n", INDIV);
    fflush(svrwfp);

    if (verbose)
        fprintf(stderr, "Queried properties from server\n");
}

/* listen for INDI traffic on svrrfp.
 * collect device names and return on timeout.
 */
static void listenINDI()
{
    char msg[1024];

    /* arrange to call onAlarm() on timeout */
    signal(SIGALRM, onAlarm);
    alarm(timeout);

    /* read from server until timeout */
    while (1)
    {
        XMLEle *root = readXMLEle(lillp, readServerChar(), msg);
        if (root)
        {
            /* found a complete XML element */
            if (verbose > 1)
                prXMLEle(stderr, root, 0);
            findDevices(root);
            delXMLEle(root);
        }
        else if (msg[0])
        {
            fprintf(stderr, "Bad XML from %s/%d: %s\n", host, port, msg);
            exit(2);
        }
    }
}

/* called after timeout seconds */
static void onAlarm(int dummy)
{
    (void)dummy;
    
    /* print all found devices */
    for (int i = 0; i < devlist.count; i++)
    {
        printf("%s\n", devlist.devices[i]);
    }

    /* clean up */
    for (int i = 0; i < devlist.count; i++)
        free(devlist.devices[i]);
    free(devlist.devices);
    delLilXML(lillp);
    
    exit(devlist.count > 0 ? 0 : 1);
}

/* read one char from svrrfp */
static int readServerChar()
{
    int c = fgetc(svrrfp);

    if (c == EOF)
    {
        if (ferror(svrrfp))
            perror("read");
        else
            fprintf(stderr, "INDI server %s/%d disconnected\n", host, port);
        exit(2);
    }

    if (verbose > 2)
        fprintf(stderr, "Read %c\n", c);

    return (c);
}

/* extract device name from root if it's a defXXXVector */
static void findDevices(XMLEle *root)
{
    const char *tag = tagXMLEle(root);

    /* check if this is a definition vector */
    for (int i = 0; i < ndefs; i++)
    {
        if (strcmp(tag, defs[i].vec) == 0)
        {
            /* found a defXXXVector, get device name */
            const char *dev = findXMLAttValu(root, "device");
            if (dev && dev[0])
            {
                addDevice(dev);
                /* reset alarm since we got new data */
                alarm(timeout);
            }
            break;
        }
    }
}
