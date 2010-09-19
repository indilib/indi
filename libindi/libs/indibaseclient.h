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
    INDIBaseClient();
    virtual ~INDIBaseClient();

    void setServer(string serverAddress, unsigned int port);
    void addDevice(string deviceName);
    bool connect();
    INDIBaseDevice * getDevice(string deviceName);

    // Update
    static void * listenHelper(void *context);

protected:



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
