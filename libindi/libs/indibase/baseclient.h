#ifndef INDIBASECLIENT_H
#define INDIBASECLIENT_H

#include <vector>
#include <map>
#include <string>

#include "indiapi.h"
#include "indidevapi.h"
#include "indibase.h"

#define MAXRBUF 2048

using namespace std;

class INDI::BaseClient : public INDI::BaseMediator
{
public:
    enum { INDI_DEVICE_NOT_FOUND=-1, INDI_PROPERTY_INVALID=-2, INDI_PROPERTY_DUPLICATED = -3, INDI_DISPATCH_ERROR=-4 };

    BaseClient();
    virtual ~BaseClient();

    void setServer(const char * hostname, unsigned int port);

    // Add devices to watch.
    void watchDevice(const char * deviceName);

    // Connect/Disconnect to/from INDI server
    bool connect();
    void disconnect();

    // Return instance of BaseDevice
    INDI::BaseDevice * getDevice(const char * deviceName);
    const vector<INDI::BaseDevice *> & getDevices() const { return cDevices; }

    void setBLOBMode(BLOBHandling blobH, const char *dev = NULL, const char *prop = NULL);

    // Update
    static void * listenHelper(void *context);

protected:

    int dispatchCommand(XMLEle *root, char* errmsg);

    // Remove devices & properties
    int removeDevice( const char * devName, char * errmsg );
    int delPropertyCmd (XMLEle *root, char * errmsg);

    // Process devices
    INDI::BaseDevice * findDev( const char * devName, char * errmsg);
    INDI::BaseDevice * addDevice (XMLEle *dep, char * errmsg);
    INDI::BaseDevice * findDev (XMLEle *root, int create, char * errmsg);

    // Process messages
    int messageCmd (XMLEle *root, char * errmsg);
    void checkMsg (XMLEle *root, INDI::BaseDevice *dp);
    void doMsg (XMLEle *msg, INDI::BaseDevice *dp);

    // Send newXXX to driver
    void sendNewText (ITextVectorProperty *pp);
    void sendNewNumber (INumberVectorProperty *pp);
    void sendNewSwitch (ISwitchVectorProperty *pp, ISwitch *lp);
    void startBlob( const char *devName, const char *propName, const char *timestamp);
    void sendOneBlob( const char *blobName, unsigned int blobSize, const char *blobFormat, unsigned char * blobBuffer);
    void finishBlob();

private:

    // Listen to INDI server and process incoming messages
    void listenINDI();

    // Thread for listenINDI()
    pthread_t listen_thread;

    vector<INDI::BaseDevice *> cDevices;
    vector<string> cDeviceNames;

    string cServer;
    unsigned int cPort;

    // Parse & FILE buffers for IO
    int sockfd;
    LilXML *lillp;			/* XML parser context */
    FILE *svrwfp;			/* FILE * to talk to server */
    FILE *svrrfp;			/* FILE * to read from server */

};

#endif // INDIBASECLIENT_H
