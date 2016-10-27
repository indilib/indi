/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

 INDI Qt Client

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <fcntl.h>
#include <locale.h>
#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#endif

#include "baseclientqt.h"
#include "basedevice.h"
#include "indicom.h"

#include <errno.h>

#define MAXINDIBUF 49152

#if defined(  _MSC_VER )
#define snprintf _snprintf
#pragma warning(push)
///@todo Introduce plattform indipendent safe functions as macros to fix this
#pragma warning(disable: 4996)
#endif

INDI::BaseClientQt::BaseClientQt()
{
    cServer = "localhost";
    cPort   = 7624;
    sConnected = false;
    verbose = false;
    lillp=NULL;

    timeout_sec=3;
    timeout_us=0;

    connect(&client_socket, SIGNAL(readyRead()), this, SLOT(listenINDI()));
    connect(&client_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(processSocketError(QAbstractSocket::SocketError)));
}

INDI::BaseClientQt::~BaseClientQt()
{
}


void INDI::BaseClientQt::setServer(const char * hostname, unsigned int port)
{
    cServer = hostname;
    cPort   = port;
}

void INDI::BaseClientQt::watchDevice(const char * deviceName)
{
    cDeviceNames.push_back(deviceName);
}

bool INDI::BaseClientQt::connectServer()
{
    client_socket.connectToHost(cServer.c_str(), cPort);

    if (client_socket.waitForConnected(timeout_sec*1000) == false)
    {
        sConnected = false;
        return false;
    }

    lillp = newLilXML();

    sConnected = true;

    serverConnected();

    char *orig = setlocale(LC_NUMERIC,"C");
    QString getProp;
    if (cDeviceNames.empty())
    {
       getProp = QString("<getProperties version='%1'/>\n").arg(QString::number(INDIV));

       client_socket.write(getProp.toLatin1());

       if (verbose)
           std::cerr << getProp.toLatin1().constData() << std::endl;
    }
    else
    {
        vector<string>::const_iterator stri;
        for ( stri = cDeviceNames.begin(); stri != cDeviceNames.end(); stri++)
        {
            getProp = QString("<getProperties version='%1' device='%2'/>\n").arg(QString::number(INDIV)).arg( (*stri).c_str());

            client_socket.write(getProp.toLatin1());
            if (verbose)
                std::cerr << getProp.toLatin1().constData() << std::endl;
        }
    }
    setlocale(LC_NUMERIC,orig);

    return true;
}

bool INDI::BaseClientQt::disconnectServer()
{
    if (sConnected == false)
        return true;

    sConnected = false;

    client_socket.close();
    if (lillp)
    {
       delLilXML(lillp);
       lillp=NULL;
    }

    cDevices.clear();
    cDeviceNames.clear();

    return true;
}

void INDI::BaseClientQt::connectDevice(const char *deviceName)
{
    setDriverConnection(true, deviceName);
}

void INDI::BaseClientQt::disconnectDevice(const char *deviceName)
{
    setDriverConnection(false, deviceName);
}

void INDI::BaseClientQt::setDriverConnection(bool status, const char *deviceName)
{
    INDI::BaseDevice *drv = getDevice(deviceName);
    ISwitchVectorProperty *drv_connection = NULL;

    if (drv == NULL)
    {
        IDLog("INDI::BaseClientQt: Error. Unable to find driver %s\n", deviceName);
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

        sendNewSwitch(drv_connection);
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

        sendNewSwitch(drv_connection);

    }
}

INDI::BaseDevice * INDI::BaseClientQt::getDevice(const char * deviceName)
{
    vector<INDI::BaseDevice *>::const_iterator devi;
    for ( devi = cDevices.begin(); devi != cDevices.end(); ++devi)
        if (!strcmp(deviceName, (*devi)->getDeviceName()))
            return (*devi);

    return NULL;
}

void * INDI::BaseClientQt::listenHelper(void *context)
{
  (static_cast<INDI::BaseClientQt *> (context))->listenINDI();
  return NULL;
}

void INDI::BaseClientQt::listenINDI()
{
    char buffer[MAXINDIBUF];
    char errorMsg[MAXRBUF];
    int err_code=0;
    
    XMLEle **nodes;
    XMLEle *root;
    int inode=0;

    if (sConnected == false)
        return;

    while (client_socket.bytesAvailable() > 0)
    {
        qint64 readBytes = client_socket.read(buffer, MAXINDIBUF - 1);
        if ( readBytes > 0 )
            buffer[ readBytes ] = '\0';
	
	nodes=parseXMLChunk(lillp, buffer, readBytes, errorMsg);
	if (!nodes) {
	  if (errorMsg[0])
	    {
	      fprintf (stderr, "Bad XML from %s/%d: %s\n%s\n", cServer.c_str(), cPort, errorMsg, buffer);
	      return;
	    }
	  return;
	}
	root=nodes[inode];
	while (root)
	  {
	    if (verbose)
	      prXMLEle(stderr, root, 0);
	    
	    if ( (err_code = dispatchCommand(root, errorMsg)) < 0)
	      {
		// Silenty ignore property duplication errors
		if (err_code != INDI_PROPERTY_DUPLICATED)
		  {
		    IDLog("Dispatch command error(%d): %s\n", err_code, errorMsg);
		    prXMLEle (stderr, root, 0);
		  }
	      }
	    
	    
	    delXMLEle (root);	// not yet, delete and continue
	    inode++; root=nodes[inode];
	  }
	free(nodes);
	inode=0;
    }
}

int INDI::BaseClientQt::dispatchCommand(XMLEle *root, char * errmsg)
{
    if  (!strcmp (tagXMLEle(root), "message"))
        return messageCmd(root, errmsg);
    else if  (!strcmp (tagXMLEle(root), "delProperty"))
        return delPropertyCmd(root, errmsg);
    // Just ignore any getProperties we might get
    else if  (!strcmp (tagXMLEle(root), "getProperties"))
        return INDI_PROPERTY_DUPLICATED;

    /* Get the device, if not available, create it */
    INDI::BaseDevice *dp = findDev (root, 1, errmsg);
    if (dp == NULL)
    {
        strcpy(errmsg,"No device available and none was created");
        return INDI_DEVICE_NOT_FOUND;
    }

    // FIXME REMOVE THIS

    // Ignore echoed newXXX and getProperties
    if (strstr(tagXMLEle(root), "new") ||strstr(tagXMLEle(root), "getProperties"))
        return 0;

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
int INDI::BaseClientQt::delPropertyCmd (XMLEle *root, char * errmsg)
{
    XMLAtt *ap;
    INDI::BaseDevice *dp;

    /* dig out device and optional property name */
    dp = findDev (root, 0, errmsg);
    if (!dp)
        return INDI_DEVICE_NOT_FOUND;

    dp->checkMessage(root);

    ap = findXMLAtt (root, "name");

    /* Delete property if it exists, otherwise, delete the whole device */
    if (ap)
    {
        INDI::Property *rProp = dp->getProperty(valuXMLAtt(ap));
        removeProperty(rProp);
        int errCode = dp->removeProperty(valuXMLAtt(ap), errmsg);

        return errCode;
    }
    // delete the whole device
    else
        return deleteDevice(dp->getDeviceName(), errmsg);
}

int INDI::BaseClientQt::deleteDevice( const char * devName, char * errmsg )
{
    std::vector<INDI::BaseDevice *>::iterator devicei;

    for (devicei =cDevices.begin(); devicei != cDevices.end();)
    {

      if (!strcmp(devName, (*devicei)->getDeviceName()))
      {
          removeDevice(*devicei);
          delete *devicei;
          devicei = cDevices.erase(devicei);
          return 0;
      }
      else
          ++devicei;
    }

    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return INDI_DEVICE_NOT_FOUND;
}

INDI::BaseDevice * INDI::BaseClientQt::findDev( const char * devName, char * errmsg )
{
    std::vector<INDI::BaseDevice *>::const_iterator devicei;

    for (devicei = cDevices.begin(); devicei != cDevices.end(); devicei++)
    {
        if (!strcmp(devName, (*devicei)->getDeviceName()))
         return (*devicei);

    }

    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return NULL;
}

/* add new device */
INDI::BaseDevice * INDI::BaseClientQt::addDevice (XMLEle *dep, char * errmsg)
{
    //devicePtr dp(new INDI::BaseDriver());
    INDI::BaseDevice *dp = new INDI::BaseDevice();
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

    newDevice(dp);

    /* ok */
    return dp;
}

INDI::BaseDevice * INDI::BaseClientQt::findDev (XMLEle *root, int create, char * errmsg)
{
    XMLAtt *ap;
    INDI::BaseDevice *dp;
    char *dn;

    /* get device name */
    ap = findXMLAtt (root, "device");
    if (!ap)
    {
        snprintf(errmsg, MAXRBUF, "No device attribute found in element %s", tagXMLEle(root));
        return (NULL);
    }

    dn = valuXMLAtt(ap);

    if (*dn == '\0')
    {
        snprintf(errmsg, MAXRBUF, "Device name is empty! %s", tagXMLEle(root));
        return (NULL);
    }

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
int INDI::BaseClientQt::messageCmd (XMLEle *root, char * errmsg)
{
    INDI::BaseDevice *dp =findDev (root, 0, errmsg);

    if (dp)
        dp->checkMessage(root);

    return (0);
}


void INDI::BaseClientQt::sendNewText (ITextVectorProperty *tvp)
{
    char *orig = setlocale(LC_NUMERIC,"C");

    tvp->s = IPS_BUSY;

    QString prop;

    prop += QString("<newTextVector\n");
    prop += QString("  device='%1'\n").arg(tvp->device);
    prop += QString("  name='%1'\n>").arg(tvp->name);

    for (int i=0; i < tvp->ntp; i++)
    {
        prop += QString("  <oneText\n");
        prop += QString("    name='%1'>\n").arg(tvp->tp[i].name);
        prop += QString("      %1\n").arg(tvp->tp[i].text);
        prop += QString("  </oneText>\n");
    }
    prop += QString("</newTextVector>\n");

    client_socket.write(prop.toLatin1());

    setlocale(LC_NUMERIC,orig);
}

void INDI::BaseClientQt::sendNewText (const char * deviceName, const char * propertyName, const char* elementName, const char *text)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (drv == NULL)
        return;

    ITextVectorProperty *tvp = drv->getText(propertyName);

    if (tvp == NULL)
        return;

    IText * tp = IUFindText(tvp, elementName);

    if (tp == NULL)
        return;

    IUSaveText(tp, text);

    sendNewText(tvp);
}

void INDI::BaseClientQt::sendNewNumber (INumberVectorProperty *nvp)
{
   char *orig = setlocale(LC_NUMERIC,"C");

    nvp->s = IPS_BUSY;

    QString prop;

    prop += QString("<newNumberVector\n");
    prop += QString("  device='%1'\n").arg(nvp->device);
    prop += QString("  name='%1'\n>").arg(nvp->name);

    for (int i=0; i < nvp->nnp; i++)
    {
        prop += QString("  <oneNumber\n");
        prop += QString("    name='%1'>\n").arg(nvp->np[i].name);
        prop += QString("      %1\n").arg(QString::number(nvp->np[i].value));
        prop += QString("  </oneNumber>\n");
    }
    prop += QString("</newNumberVector>\n");

    client_socket.write(prop.toLatin1());

   setlocale(LC_NUMERIC,orig);
}

void INDI::BaseClientQt::sendNewNumber (const char *deviceName, const char *propertyName, const char* elementName, double value)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (drv == NULL)
        return;

    INumberVectorProperty *nvp = drv->getNumber(propertyName);

    if (nvp == NULL)
        return;

    INumber * np = IUFindNumber(nvp, elementName);

    if (np == NULL)
        return;

    np->value = value;

    sendNewNumber(nvp);

}

void INDI::BaseClientQt::sendNewSwitch (ISwitchVectorProperty *svp)
{
    svp->s = IPS_BUSY;
    ISwitch *onSwitch = IUFindOnSwitch(svp);

    QString prop;

    prop += QString("<newSwitchVector\n");

    prop += QString("  device='%1'\n").arg(svp->device);
    prop += QString("  name='%1'>\n").arg(svp->name);

    if (svp->r == ISR_1OFMANY && onSwitch)
    {
        prop += QString("  <oneSwitch\n");
        prop += QString("    name='%1'>\n").arg(onSwitch->name);
        prop += QString("      %1\n").arg((onSwitch->s == ISS_ON) ? "On" : "Off");
        prop += QString("  </oneSwitch>\n");
    }
    else
    {
        for (int i=0; i < svp->nsp; i++)
        {
            prop += QString("  <oneSwitch\n");
            prop += QString("    name='%1'>\n").arg(svp->sp[i].name);
            prop += QString("      %1\n").arg((svp->sp[i].s == ISS_ON) ? "On" : "Off");
            prop += QString("  </oneSwitch>\n");
        }
    }

    prop += QString("</newSwitchVector>\n");

    client_socket.write(prop.toLatin1());
}

void INDI::BaseClientQt::sendNewSwitch (const char *deviceName, const char *propertyName, const char *elementName)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (drv == NULL)
        return;

    ISwitchVectorProperty *svp = drv->getSwitch(propertyName);

    if (svp == NULL)
        return;

    ISwitch * sp = IUFindSwitch(svp, elementName);

    if (sp == NULL)
        return;

    sp->s = ISS_ON;

    sendNewSwitch(svp);

}

void INDI::BaseClientQt::startBlob( const char *devName, const char *propName, const char *timestamp)
{
    QString prop;

    prop += QString("<newBLOBVector\n");
    prop += QString("  device='%1'\n").arg(devName);
    prop += QString("  name='%1'\n").arg(propName);
    prop += QString("  timestamp='%1'>\n").arg(timestamp);

    client_socket.write(prop.toLatin1());
}

void INDI::BaseClientQt::sendOneBlob( const char *blobName, unsigned int blobSize, const char *blobFormat, void * blobBuffer)
{
    QString prop;

    prop += QString("  <oneBLOB\n");
    prop += QString("    name='%1'\n").arg(blobName);
    prop += QString("    size='%1'\n").arg(QString::number(blobSize));
    prop += QString("    format='%1'>\n").arg(blobFormat);

    client_socket.write(prop.toLatin1());

    client_socket.write(static_cast<char *>(blobBuffer), blobSize);

    client_socket.write("   </oneBLOB>\n");
}

void INDI::BaseClientQt::finishBlob()
{
    client_socket.write("</newBLOBVector>\n");
}

void INDI::BaseClientQt::setBLOBMode(BLOBHandling blobH, const char *dev, const char *prop)
{
    if (!dev[0])
        return;

    QString blobOpenTag;
    QString blobEnableTag;
    if (prop != NULL)
        blobOpenTag = QString("<enableBLOB device='%1' name='%2'>").arg(dev).arg(prop);
    else
        blobOpenTag = QString("<enableBLOB device='%1'>").arg(dev);

     switch (blobH)
     {
     case B_NEVER:
         blobEnableTag = QString("%1Never</enableBLOB>\n").arg(blobOpenTag);
         break;
     case B_ALSO:
         blobEnableTag = QString("%1Also</enableBLOB>\n").arg(blobOpenTag);
         break;
     case B_ONLY:
         blobEnableTag = QString("%1Only</enableBLOB>\n").arg(blobOpenTag);
         break;
     }

     client_socket.write(blobEnableTag.toLatin1());
}

void INDI::BaseClientQt::processSocketError( QAbstractSocket::SocketError socketError )
{
    if (sConnected == false)
        return;

    // TODO Handle what happens on socket failure!
    INDI_UNUSED(socketError);
    IDLog("Socket Error: %s\n", client_socket.errorString().toLatin1().constData());
    fprintf (stderr,"INDI server %s/%d disconnected.\n", cServer.c_str(), cPort);
    disconnectServer();
    serverDisconnected(-1);
}

#if defined(  _MSC_VER )
#undef snprintf
#pragma warning(pop)
#endif
