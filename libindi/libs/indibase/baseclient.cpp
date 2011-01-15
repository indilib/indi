#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "baseclient.h"
#include "basedriver.h"
#include "indicom.h"

#include <errno.h>

#define MAXINDIBUF 64

INDI::BaseClient::BaseClient()
{
    cServer = "localhost";
    cPort   = 7624;
    svrwfp = NULL;    
    sConnected = false;

}


INDI::BaseClient::~BaseClient()
{
    close(sockfd);
}

void INDI::BaseClient::setServer(const char * hostname, unsigned int port)
{
    cServer = hostname;
    cPort   = port;

}

void INDI::BaseClient::watchDevice(const char * deviceName)
{
    cDeviceNames.push_back(deviceName);
}

bool INDI::BaseClient::connectServer()
{
    struct sockaddr_in serv_addr;
    struct hostent *hp;

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

    int result = pthread_create( &listen_thread, NULL, &INDI::BaseClient::listenHelper, this);

    if (result != 0)
    {
        perror("thread");
        return false;
    }

    serverConnected();
    sConnected = true;

    return true;
}

bool INDI::BaseClient::disconnectServer()
{
    if (sConnected == false)
        return true;

    if (svrwfp != NULL)
        fclose(svrwfp);
   svrwfp = NULL;

   close(sockfd);

    serverDisconnected();

    sConnected = false;

    return true;
}


void INDI::BaseClient::setDriverConnection(bool status, const char *deviceName)
{
    INDI::BaseDriver *drv = getDriver(deviceName);
    ISwitchVectorProperty *drv_connection = NULL;

    if (drv == NULL)
    {
        IDLog("INDI::BaseClient: Error. Unable to find driver %s\n", deviceName);
        return;
    }

    drv_connection = drv->getSwitch("CONNECTION");

    if (drv_connection == NULL)
        return;

    // If we need to connect
    if (status)
    {
        // If there is no need to do anything, i.e. already connected.
        if (drv_connection->sp[0].s == ISS_ON)
            return;

        IUResetSwitch(drv_connection);
        drv_connection->s = IPS_BUSY;
        drv_connection->sp[0].s = ISS_ON;
        drv_connection->sp[1].s = ISS_OFF;

        sendNewSwitch(drv_connection, &(drv_connection->sp[0]));
    }
    else
    {
        // If there is no need to do anything, i.e. already disconnected.
        if (drv_connection->sp[1].s == ISS_ON)
            return;

        IUResetSwitch(drv_connection);
        drv_connection->s = IPS_BUSY;
        drv_connection->sp[0].s = ISS_OFF;
        drv_connection->sp[1].s = ISS_ON;

        sendNewSwitch(drv_connection, &(drv_connection->sp[1]));

    }
}


INDI::BaseDriver * INDI::BaseClient::getDriver(const char * deviceName)
{
    vector<devicePtr>::const_iterator devi;
    for ( devi = cDevices.begin(); devi != cDevices.end(); devi++)
        if (!strcmp(deviceName, (*devi)->deviceName()))
            return (*devi).get();

    return NULL;
}

void * INDI::BaseClient::listenHelper(void *context)
{
  (static_cast<INDI::BaseClient *> (context))->listenINDI();
}

void INDI::BaseClient::listenINDI()
{
    char msg[MAXRBUF];
    char buffer[MAXINDIBUF];
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

    lillp = newLilXML();

    /* read from server, exit if find all requested properties */
    while (sConnected)
    {
        n = read(sockfd, buffer, MAXINDIBUF);

        if (n ==0)
        {
            perror ("read");
            fprintf (stderr,"INDI server %s/%d disconnected\n", cServer.c_str(), cPort);
            serverDisconnected();
            return;
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
                         IDLog("Dispatch command error: %s\n", msg);
                         prXMLEle (stderr, root, 0);
                     }
                }

               delXMLEle (root);	/* not yet, delete and continue */
            }
            else if (msg[0])
            {
               fprintf (stderr, "Bad XML from %s/%d: %s\n%s\n", cServer.c_str(), cPort, msg, buffer);
               return;
            }
        }
    }

}

int INDI::BaseClient::dispatchCommand(XMLEle *root, char * errmsg)
{
    if  (!strcmp (tagXMLEle(root), "message"))
        return messageCmd(root, errmsg);
    else if  (!strcmp (tagXMLEle(root), "delProperty"))
        return delPropertyCmd(root, errmsg);

    /* Get the device, if not available, create it */
    INDI::BaseDriver *dp = findDev (root, 1, errmsg);
    if (dp == NULL)
    {
        strcpy(errmsg,"No device available and none was created");
        return INDI_DEVICE_NOT_FOUND;
    }

    if ((!strcmp (tagXMLEle(root), "defTextVector"))  ||
       (!strcmp (tagXMLEle(root), "defNumberVector")) ||
       (!strcmp (tagXMLEle(root), "defSwitchVector")) ||
       (!strcmp (tagXMLEle(root), "defLightVector"))  ||
       (!strcmp (tagXMLEle(root), "defBLOBVector")))
        return dp->buildProp(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "setTextVector") ||
             !strcmp (tagXMLEle(root), "setNumberVector") ||
             !strcmp (tagXMLEle(root), "setSwitchVector") ||
             !strcmp (tagXMLEle(root), "setLightVector") ||
             !strcmp (tagXMLEle(root), "setBLOBVector"))
            return dp->setValue(root, errmsg);

    return INDI_DISPATCH_ERROR;
}

/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDI::BaseClient::delPropertyCmd (XMLEle *root, char * errmsg)
{
    XMLAtt *ap;
    INDI::BaseDriver *dp;

    /* dig out device and optional property name */
    dp = findDev (root, 0, errmsg);
    if (!dp)
        return INDI_DEVICE_NOT_FOUND;

    checkMsg(root, dp);

    ap = findXMLAtt (root, "name");

    /* Delete property if it exists, otherwise, delete the whole device */
    if (ap)
        return dp->removeProperty(valuXMLAtt(ap));
    // delete the whole device
    else
        return removeDevice(dp->deviceName(), errmsg);
}

int INDI::BaseClient::removeDevice( const char * devName, char * errmsg )
{
    std::vector<devicePtr>::iterator devicei = cDevices.begin();

    while (devicei != cDevices.end())
    {
      if (strcmp(devName, (*devicei)->deviceName()))
      {
          cDevices.erase(devicei);
          //delete (*devicei);
          return 0;
      }

      devicei++;
    }

    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return INDI_DEVICE_NOT_FOUND;
}

INDI::BaseDriver * INDI::BaseClient::findDev( const char * devName, char * errmsg )
{

    std::vector<devicePtr>::const_iterator devicei;

    for (devicei = cDevices.begin(); devicei != cDevices.end(); devicei++)
    {
        if (!strcmp(devName, (*devicei)->deviceName()))
         return (*devicei).get();

    }

    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return NULL;
}

/* add new device */
INDI::BaseDriver * INDI::BaseClient::addDevice (XMLEle *dep, char * errmsg)
{
    devicePtr dp(new INDI::BaseDriver());
    XMLAtt *ap;
    char * device_name;

    /* allocate new INDI::BaseDriver */
    ap = findXMLAtt (dep, "device");
    if (!ap)
    {
        strncpy(errmsg, "Unable to find device attribute in XML element. Cannot add device.", MAXRBUF);
        return NULL;
    }

    device_name = valuXMLAtt(ap);

    dp->setMediator(this);
    dp->setDeviceName(device_name);

    cDevices.push_back(dp);

    newDevice(device_name);

    /* ok */
    return dp.get();
}

INDI::BaseDriver * INDI::BaseClient::findDev (XMLEle *root, int create, char * errmsg)
{
    XMLAtt *ap;
    INDI::BaseDriver *dp;
    char *dn;

    /* get device name */
    ap = findXMLAtt (root, "device");
    if (!ap)
    {
        snprintf(errmsg, MAXRBUF, "No device attribute found in element %s", tagXMLEle(root));
        return (NULL);
    }

    dn = valuXMLAtt(ap);

    dp = findDev(dn, errmsg);

    if (dp)
        return dp;

    /* not found, create if ok */
    if (create)
        return (addDevice (root, errmsg));

    snprintf(errmsg, MAXRBUF, "INDI: <%s> no such device %s", tagXMLEle(root), dn);
    return NULL;
}

/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDI::BaseClient::messageCmd (XMLEle *root, char * errmsg)
{
    checkMsg (root, findDev (root, 0, errmsg));
    return (0);
}

/* add message to queue
 * N.B. don't put carriage control in msg, we take care of that.
 */
void INDI::BaseClient::checkMsg (XMLEle *root, INDI::BaseDriver *dp)
{
    XMLAtt *ap;
    ap = findXMLAtt(root, "message");

    if (ap)
        doMsg(root, dp);
}

/* Store msg in queue */
void INDI::BaseClient::doMsg (XMLEle *msg, INDI::BaseDriver *dp)
{
    XMLAtt *message;
    XMLAtt *time_stamp;
    char msgBuffer[MAXRBUF];

    if (dp == NULL)
        return;

    /* prefix our timestamp if not with msg */
    time_stamp = findXMLAtt (msg, "timestamp");

    if (time_stamp)
        snprintf(msgBuffer, MAXRBUF, "%s ", valuXMLAtt(time_stamp));
    else
        snprintf(msgBuffer, MAXRBUF, "%s ", timestamp());

    /* finally! the msg */
    message = findXMLAtt(msg, "message");

    if (!message) return;

    // Prepend to the log
   dp->addMessage(msgBuffer);
   dp->addMessage(valuXMLAtt(message));
   dp->addMessage("\n");
\
}

void INDI::BaseClient::sendNewText (ITextVectorProperty *tvp)
{
    fprintf(svrwfp, "<newTextVector\n");
    fprintf(svrwfp, "  device='%s'\n", tvp->device);
    fprintf(svrwfp, "  name='%s'\n>", tvp->name);

    for (int i=0; i < tvp->ntp; i++)
    {
        fprintf(svrwfp, "  <oneText\n");
        fprintf(svrwfp, "    name='%s'>\n", tvp->tp[i].name);
        fprintf(svrwfp, "      %s\n", tvp->tp[i].text);
        fprintf(svrwfp, "  </oneText>\n");
    }
    fprintf(svrwfp, "</newTextVector>\n");

    fflush(svrwfp);
}

void INDI::BaseClient::sendNewNumber (INumberVectorProperty *nvp)
{
    fprintf(svrwfp, "<newNumberVector\n");
    fprintf(svrwfp, "  device='%s'\n", nvp->device);
    fprintf(svrwfp, "  name='%s'\n>", nvp->name);

    for (int i=0; i < nvp->nnp; i++)
    {
        fprintf(svrwfp, "  <oneNumber\n");
        fprintf(svrwfp, "    name='%s'>\n", nvp->np[i].name);
        fprintf(svrwfp, "      %g\n", nvp->np[i].value);
        fprintf(svrwfp, "  </oneNumber>\n");
    }
    fprintf(svrwfp, "</newNumberVector>\n");

   fflush(svrwfp);
}

void INDI::BaseClient::sendNewSwitch (ISwitchVectorProperty *svp, ISwitch *sp)
{
    fprintf(svrwfp, "<newSwitchVector\n");

    fprintf(svrwfp, "  device='%s'\n", svp->device);
    fprintf(svrwfp, "  name='%s'>\n", svp->name);
    fprintf(svrwfp, "  <oneSwitch\n");
    fprintf(svrwfp, "    name='%s'>\n", sp->name);
    fprintf(svrwfp, "      %s\n", (sp->s == ISS_ON) ? "On" : "Off");
    fprintf(svrwfp, "  </oneSwitch>\n");

    fprintf(svrwfp, "</newSwitchVector>\n");

    fflush(svrwfp);
}

void INDI::BaseClient::startBlob( const char *devName, const char *propName, const char *timestamp)
{
    fprintf(svrwfp, "<newBLOBVector\n");
    fprintf(svrwfp, "  device='%s'\n", devName);
    fprintf(svrwfp, "  name='%s'\n", propName);
    fprintf(svrwfp, "  timestamp='%s'>\n",  timestamp);
}

void INDI::BaseClient::sendOneBlob( const char *blobName, unsigned int blobSize, const char *blobFormat, unsigned char * blobBuffer)
{
    fprintf(svrwfp, "  <oneBLOB\n");
    fprintf(svrwfp, "    name='%s'\n", blobName);
    fprintf(svrwfp, "    size='%d'\n", blobSize);
    fprintf(svrwfp, "    format='%s'>\n", blobFormat);

    for (unsigned i = 0; i < blobSize; i += 72)
        fprintf(svrwfp, "    %.72s\n", blobBuffer+i);

    fprintf(svrwfp, "   </oneBLOB>\n");
}

void INDI::BaseClient::finishBlob()
{
    fprintf(svrwfp, "</newBLOBVector>\n");
    fflush(svrwfp);

}

void INDI::BaseClient::setBLOBMode(BLOBHandling blobH, const char *dev, const char *prop)
{
    char blobOpenTag[64];

    if (!dev[0])
        return;

   if (prop[0])
           snprintf(blobOpenTag, 64, "<enableBLOB device='%s' name='%s'>", dev, prop);
   else
          snprintf(blobOpenTag, 64, "<enableBLOB device='%s'>", dev);

    switch (blobH)
    {
    case B_NEVER:
        fprintf(svrwfp, "%sNever</enableBLOB>\n", blobOpenTag);
        break;
    case B_ALSO:
        fprintf(svrwfp, "%sAlso</enableBLOB>\n", blobOpenTag);
        break;
    case B_ONLY:
        fprintf(svrwfp, "%sOnly</enableBLOB>\n", blobOpenTag);
        break;
    }

    fflush(svrwfp);
}



