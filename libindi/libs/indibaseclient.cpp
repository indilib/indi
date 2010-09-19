#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "indidrivermain.h"
#include "indibaseclient.h"
#include "indicom.h"

#include <errno.h>

INDIBaseClient::INDIBaseClient()
{
    cServer = "localhost";
    cPort   = 7624;

    lillp = newLilXML();

}


INDIBaseClient::~INDIBaseClient()
{

    delLilXML(lillp);

}

void INDIBaseClient::setServer(string serverAddress, unsigned int port)
{
    cServer = serverAddress;
    cPort   = port;

}

void INDIBaseClient::addDevice(string deviceName)
{
    cDeviceNames.push_back(deviceName);
}

bool INDIBaseClient::connect()
{
    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int sockfd;

    /* lookup host address */
    hp = gethostbyname(cServer.c_str());
    if (!hp)
    {
        herror ("gethostbyname");
        return false;
    }

    /* create a socket to the INDI server */
    (void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port = htons(cPort);
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror ("socket");
        return false;
    }

    /* connect */
    if (::connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        perror ("connect");
        return false;
    }

    /* prepare for line-oriented i/o with client */
    svrwfp = fdopen (sockfd, "w");
    svrrfp = fdopen (sockfd, "r");

    int result = pthread_create( &listen_thread, NULL, &INDIBaseClient::listenHelper, this);

    if (result != 0)
    {
        perror("thread");
        return false;
    }

    return true;
}

INDIBaseDevice * INDIBaseClient::getDevice(string deviceName)
{


    return NULL;
}

void * INDIBaseClient::listenHelper(void *context)
{
  (static_cast<INDIBaseClient *> (context))->listenINDI();
}

void INDIBaseClient::listenINDI()
{
    char msg[1024];
    char buffer[MAXRBUF];
    int n=0;

    if (cDeviceNames.empty())
       fprintf(svrwfp, "<getProperties version='%g'/>\n", INDIV);
    else
    {
        vector<string>::const_iterator stri;
        for ( stri = cDeviceNames.begin(); stri != cDeviceNames.end(); stri++)
            fprintf(svrwfp, "<getProperties version='%g' device='%s'/>\n", INDIV, (*stri).c_str());
    }

    fflush (svrwfp);

    /* read from server, exit if find all requested properties */
    while (1)
    {
        n = fread(buffer, 1, MAXRBUF, svrrfp);

        if (n ==0)
        {
            if (ferror(svrrfp))
                perror ("read");
            else
                fprintf (stderr,"INDI server %s/%d disconnected\n", cServer.c_str(), cPort);
            exit (2);
        }

        for (int i=0; i < n; i++)
        {
            XMLEle *root = readXMLEle (lillp, buffer[i], msg);
            if (root)
            {
               prXMLEle (stderr, root, 0);
               delXMLEle (root);	/* not yet, delete and continue */
            }
            else if (msg[0])
            {
               fprintf (stderr, "Bad XML from %s/%d: %s\n", cServer.c_str(), cPort, msg);
               exit(2);
            }
        }
    }
}
