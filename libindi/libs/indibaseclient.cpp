#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "indidrivermain.h"
#include "indibaseclient.h"
#include "indibasedevice.h"
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
    int n=0, err_code=0;

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
                if ( (err_code = dispatchCommand(root, msg)) < 0)
                {
                     // Silenty ignore property duplication errors
                     if (err_code != INDI_PROPERTY_DUPLICATED)
                     {
                         //std::cerr << "Dispatch command error: " << msg << endl;
                         prXMLEle (stderr, root, 0);
                     }
                }

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

int INDIBaseClient::dispatchCommand(XMLEle *root, char * errmsg)
{
    if  (!strcmp (tagXMLEle(root), "message"))
        return messageCmd(root, errmsg);
    else if  (!strcmp (tagXMLEle(root), "delProperty"))
        return delPropertyCmd(root, errmsg);

    /* Get the device, if not available, create it */
    INDIBaseDevice *dp = findDev (root, 1, errmsg);
    if (dp == NULL)
    {
        strcpy(errmsg,"No device available and none was created");
        return INDI_DEVICE_NOT_FOUND;
    }

    if (!strcmp (tagXMLEle(root), "defTextVector"))
        return dp->buildProp(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defNumberVector"))
        return dp->buildProp(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defSwitchVector"))
        return dp->buildProp(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defLightVector"))
        return dp->buildProp(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defBLOBVector"))
        return dp->buildProp(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "setTextVector") ||
             !strcmp (tagXMLEle(root), "setNumberVector") ||
             !strcmp (tagXMLEle(root), "setSwitchVector") ||
             !strcmp (tagXMLEle(root), "setLightVector") ||
             !strcmp (tagXMLEle(root), "setBLOBVector"))
        return dp->setAnyCmd(root, errmsg);

    return INDI_DISPATCH_ERROR;
}

/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDIBaseClient::delPropertyCmd (XMLEle *root, char * errmsg)
{
#if 0
    XMLAtt *ap;
    INDIBaseDevice *dp;
    INDI_P *pp;

    /* dig out device and optional property name */
    dp = findDev (root, 0, errmsg);
    if (!dp)
        return INDI_DEVICE_NOT_FOUND;

    checkMsg(root, dp);

    ap = findXMLAtt (root, "name");

    /* Delete property if it exists, otherwise, delete the whole device */
    if (ap)
    {
        pp = dp->findProp(char(valuXMLAtt(ap)));

        if(pp)
            return dp->removeProperty(pp);
        else
            return INDI_PROPERTY_INVALID;
    }
    // delete the whole device
    else
        return removeDevice(dp->name, errmsg);

#endif
}

int INDIBaseClient::removeDevice( const char * devName, char * errmsg )
{
#if 0
    // remove all devices if devName == NULL
    if (devName == NULL)
    {
        while ( ! INDIBaseDeviceev.isEmpty() ) delete INDIBaseDeviceev.takeFirst();
        return (0);
    }

    for (int i=0; i < INDIBaseDeviceev.size(); i++)
    {
        if (INDIBaseDeviceev[i]->name ==  devName)
        {
            delete INDIBaseDeviceev.takeAt(i);
            return (0);
        }
    }

    errmsg = char("Device %1 not found").arg(devName);

#endif
    return INDI_DEVICE_NOT_FOUND;
}

INDIBaseDevice * INDIBaseClient::findDev( const char * devName, char * errmsg )
{
#if 0
    /* search for existing */
    for (int i = 0; i < INDIBaseDeviceev.size(); i++)
    {
        if (INDIBaseDeviceev[i]->name == devName)
            return INDIBaseDeviceev[i];
    }

    errmsg = char("INDI: no such device %1").arg(devName);
#endif
    return NULL;
}

/* add new device */
INDIBaseDevice * INDIBaseClient::addDevice (XMLEle *dep, char * errmsg)
{
    INDIBaseDevice *dp;
#if 0

    XMLAtt *ap;
    char device_name, unique_label;

    /* allocate new INDIBaseDevice on INDIBaseDeviceev */
    ap = findAtt (dep, "device", errmsg);
    if (!ap)
    {
        errmsg = char("Unable to find device attribute in XML tree. Cannot add device.");
        kDebug() << errmsg << endl;
        return NULL;
    }

    device_name = char(valuXMLAtt(ap));

        if (mode != M_CLIENT)
                foreach(IDevice *device, managed_devices)
                {
                        // Each device manager has a list of managed_devices (IDevice). Each IDevice has the original constant name of the driver (driver_class)
                        // Therefore, when a new device is discovered, we match the driver name (which never changes, it's always static from indiserver) against the driver_class
                        // of IDevice because IDevice can have several names. It can have the tree_label which is the name it has in the local tree widget. Finally, the name that shows
                        // up in the INDI control panel is the unique name of the driver, which is for most cases tree_label, but if that exists already then we get tree_label_1..etc

                        if (device->driver_class == device_name && device->state == IDevice::DEV_TERMINATE)
                        {
                                device->state = IDevice::DEV_START;
                                unique_label = device->unique_label = parent->getUniqueDeviceLabel(device->tree_label);
                                break;
                        }
                }

        // For remote INDI drivers with no label attributes
        if (unique_label.isEmpty())
          unique_label = parent->getUniqueDeviceLabel(device_name);

        dp = new INDIBaseDevice(parent, this, device_name, unique_label);
        INDIBaseDeviceev.append(dp);
        emit newDevice(dp);

        connect(dp->stdDev, SIGNAL(newTelescope()), parent->ksw->indiDriver(), SLOT(newTelescopeDiscovered()), Qt::QueuedConnection);


#endif
        /* ok */
        return dp;
}

INDIBaseDevice * INDIBaseClient::findDev (XMLEle *root, int create, char * errmsg)
{
#if 0
    XMLAtt *ap;
    char *dn;

    /* get device name */
    ap = findAtt (root, "device", errmsg);
    if (!ap)
    {
        errmsg = char("No device attribute found in element %1").arg(tagXMLEle(root));
        return (NULL);
    }
    dn = valuXMLAtt(ap);

    /* search for existing */
    for (int i = 0; i < INDIBaseDeviceev.size(); i++)
    {
        if (INDIBaseDeviceev[i]->name == char(dn))
            return INDIBaseDeviceev[i];
    }

    /* not found, create if ok */
    if (create)
        return (addDevice (root, errmsg));

    errmsg = char("INDI: <%1> no such device %2").arg(tagXMLEle(root)).arg(dn);

#endif
    return NULL;
}

/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDIBaseClient::messageCmd (XMLEle *root, char * errmsg)
{
    checkMsg (root, findDev (root, 0, errmsg));
    return (0);
}

/* add message to queue
 * N.B. don't put carriage control in msg, we take care of that.
 */
void INDIBaseClient::checkMsg (XMLEle *root, INDIBaseDevice *dp)
{
    XMLAtt *ap;
    ap = findXMLAtt(root, "message");

    if (ap)
        doMsg(root, dp);
}

/* Store msg in queue */
void INDIBaseClient::doMsg (XMLEle *msg, INDIBaseDevice *dp)
{
#if 0
    QTextEdit *txt_w;
    XMLAtt *message;
    XMLAtt *timestamp;

    if (dp == NULL)
    {
        kDebug() << "Warning: dp is null.";
        return;
    }

    txt_w = dp->msgST_w;

    /* prefix our timestamp if not with msg */
    timestamp = findXMLAtt (msg, "timestamp");

    if (timestamp)
        txt_w->insertPlainText(char(valuXMLAtt(timestamp)) + char(" "));
    else
        txt_w->insertPlainText( KStarsDateTime::currentDateTime().toString("yyyy/mm/dd - h:m:s ap "));

    /* finally! the msg */
    message = findXMLAtt(msg, "message");

    if (!message) return;

    // Prepend to the log viewer
    txt_w->insertPlainText( char(valuXMLAtt(message)) + char("\n"));
    QTextCursor c = txt_w->textCursor();
    c.movePosition(QTextCursor::Start);
    txt_w->setTextCursor(c);

    if ( Options::showINDIMessages() )
        parent->ksw->statusBar()->changeItem( char(valuXMLAtt(message)), 0);
#endif
}

void INDIBaseClient::sendNewText (ITextVectorProperty *pp)
{
#if 0
    INDI_E *lp;

    QTextStream serverFP(&serverSocket);

    serverFP << char("<newTextVector\n");
    serverFP << char("  device='%1'\n").arg(qPrintable( pp->pg->dp->name));
    serverFP << char("  name='%1'\n>").arg(qPrintable( pp->name));

    //for (lp = pp->el.first(); lp != NULL; lp = pp->el.next())
    foreach(lp, pp->el)
    {
        serverFP << char("  <oneText\n");
        serverFP << char("    name='%1'>\n").arg(qPrintable( lp->name));
        serverFP << char("      %1\n").arg(qPrintable( lp->text));
        serverFP << char("  </oneText>\n");
    }
    serverFP << char("</newTextVector>\n");

#endif
}

void INDIBaseClient::sendNewNumber (INumberVectorProperty *pp)
{
#if 0
    INDI_E *lp;

    QTextStream serverFP(&serverSocket);

    serverFP << char("<newNumberVector\n");
    serverFP << char("  device='%1'\n").arg(qPrintable( pp->pg->dp->name));
    serverFP << char("  name='%1'\n>").arg(qPrintable( pp->name));

    foreach(lp, pp->el)
    {
        serverFP << char("  <oneNumber\n");
        serverFP << char("    name='%1'>\n").arg(qPrintable( lp->name));
        if (lp->text.isEmpty() || lp->spin_w)
                serverFP << char("      %1\n").arg(lp->targetValue);
        else
                serverFP << char("      %1\n").arg(lp->text);
        serverFP << char("  </oneNumber>\n");
    }
    serverFP << char("</newNumberVector>\n");

#endif

}

void INDIBaseClient::sendNewSwitch (ISwitchVectorProperty *pp, ISwitch *lp)
{
#if 0
    QTextStream serverFP(&serverSocket);

    serverFP << char("<newSwitchVector\n");
    serverFP << char("  device='%1'\n").arg(qPrintable( pp->pg->dp->name));
    serverFP << char("  name='%1'>\n").arg(qPrintable( pp->name));
    serverFP << char("  <oneSwitch\n");
    serverFP << char("    name='%1'>\n").arg(qPrintable( lp->name));
    serverFP << char("      %1\n").arg(lp->state == PS_ON ? "On" : "Off");
    serverFP << char("  </oneSwitch>\n");

    serverFP <<  char("</newSwitchVector>\n");
#endif
}

void INDIBaseClient::startBlob( const char *devName, const char *propName, const char *timestamp)
{
#if 0
    QTextStream serverFP(&serverSocket);

    serverFP <<  char("<newBLOBVector\n");
    serverFP <<  char("  device='%1'\n").arg(qPrintable( devName));
    serverFP <<  char("  name='%1'\n").arg(qPrintable( propName));
    serverFP <<  char("  timestamp='%1'>\n").arg(qPrintable( timestamp));

#endif
}

void INDIBaseClient::sendOneBlob( const char *blobName, unsigned int blobSize, const char *blobFormat, unsigned char * blobBuffer)
{
#if 0
    QTextStream serverFP(&serverSocket);

    serverFP <<  char("  <oneBLOB\n");
    serverFP <<  char("    name='%1'\n").arg(qPrintable( blobName));
    serverFP <<  char("    size='%1'\n").arg(blobSize);
    serverFP <<  char("    format='%1'>\n").arg(qPrintable( blobFormat));

    for (unsigned i = 0; i < blobSize; i += 72)
        serverFP <<  char().sprintf("    %.72s\n", blobBuffer+i);

    serverFP << char("   </oneBLOB>\n");
#endif
}

void INDIBaseClient::finishBlob()
{
#if 0
    QTextStream serverFP(&serverSocket);

    serverFP <<  char("</newBLOBVector>\n");

#endif
}



