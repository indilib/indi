#ifndef INDIBASECLIENT_H
#define INDIBASECLIENT_H

#include <vector>
#include <map>
#include <string>

#include "indiapi.h"
#include "indidevapi.h"

class INDIBaseDevice;

#define MAXRBUF 2048

using namespace std;

class INDIBaseClient
{
public:
    enum { INDI_DEVICE_NOT_FOUND=-1, INDI_PROPERTY_INVALID=-2, INDI_PROPERTY_DUPLICATED = -3, INDI_DISPATCH_ERROR=-4 };

    INDIBaseClient();
    virtual ~INDIBaseClient();

    void setServer(string serverAddress, unsigned int port);
    void addDevice(string deviceName);
    bool connect();
    INDIBaseDevice * getDevice(string deviceName);

    // Update
    static void * listenHelper(void *context);

protected:

    int dispatchCommand(XMLEle *root, char* errmsg);
    int delPropertyCmd (XMLEle *root, char * errmsg);
    int removeDevice( const char * devName, char * errmsg );
    INDIBaseDevice * findDev( const char * devName, char * errmsg);
    INDIBaseDevice * addDevice (XMLEle *dep, char * errmsg);
    INDIBaseDevice * findDev (XMLEle *root, int create, char * errmsg);
    int messageCmd (XMLEle *root, char * errmsg);
    void checkMsg (XMLEle *root, INDIBaseDevice *dp);
    void doMsg (XMLEle *msg, INDIBaseDevice *dp);
    void sendNewText (ITextVectorProperty *pp);
    void sendNewNumber (INumberVectorProperty *pp);
    void sendNewSwitch (ISwitchVectorProperty *pp, ISwitch *lp);
    void startBlob( const char *devName, const char *propName, const char *timestamp);
    void sendOneBlob( const char *blobName, unsigned int blobSize, const char *blobFormat, unsigned char * blobBuffer);
    void finishBlob();

private:

    pthread_t listen_thread;
    vector<INDIBaseDevice *> cDevices;
    vector<string> cDeviceNames;
    string cServer;
    unsigned int cPort;

    void listenINDI();

    LilXML *lillp;			/* XML parser context */
    FILE *svrwfp;			/* FILE * to talk to server */
    FILE *svrrfp;			/* FILE * to read from server */



};

#endif // INDIBASECLIENT_H
