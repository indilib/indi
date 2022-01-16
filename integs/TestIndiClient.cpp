#include <stdexcept>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <system_error>
#include <thread>

#include "gtest/gtest.h"

#include "indibase/baseclient.h"
#include "indibase/property/indiproperty.h"

#include "utils.h"

#include "SharedBuffer.h"
#include "ServerMock.h"
#include "IndiClientMock.h"


#define TEST_TCP_PORT 17624
#define TEST_UNIX_SOCKET "/tmp/indi-test-server"
#define STRINGIFY_TOK(x) #x
#define TO_STRING(x) STRINGIFY_TOK(x)

class MyClient : public INDI::BaseClient
{
   std::string dev, prop;
 public:
    MyClient(const std::string & dev, const std::string & prop) : dev(dev), prop(prop) {}
    virtual ~MyClient() {}
protected:
    virtual void newDevice(INDI::BaseDevice *dp) {
        std::cerr << "new device\n";
    }
    virtual void removeDevice(INDI::BaseDevice *dp) {
        std::cerr << "remove device\n";
    }
    virtual void newProperty(INDI::Property *property) {
        // std::cerr << "new property\n";
        /*if (dev == property->getDeviceName() && prop == property->getName()) {
            setBLOBMode(BLOBHandling::B_ONLY, dev.c_str(), prop.c_str());
        }

        if (property->getNumber()) {
            this->newNumber(property->getNumber());
        }*/
    };
    virtual void removeProperty(INDI::Property *property) {
        // std::cerr << "remove property\n";
    }
    virtual void newBLOB(IBLOB *bp) {
        std::cerr << "new blob: " << bp->format << "\n";

//         nextEntryMutex.lock();

//         // Don't leak dropped frames
//         if (nextEntryDone) {
//             fprintf(stderr, "dropping blob\n");
// #ifdef INDI_SHARED_BLOB_SUPPORT
//             IDSharedBlobFree(nextEntryData);
// #else
//             free(nextEntryData);
// #endif
//         }

//         nextEntryFormat = bp->format;
//         nextEntryData = bp->blob;
//         nextEntrySize = bp->bloblen;
//         // Free from INDI POV
//         bp->blob=nullptr;
//         bp->bloblen = 0;
//         nextEntryFrame = indiFrame;
//         nextEntryDone = true;

//         nextEntryCond.notify_all();
//         nextEntryMutex.unlock();
    }

    virtual void newSwitch(ISwitchVectorProperty *svp) {
        // std::cerr << "new switch\n";
    }

    virtual void newNumber(INumberVectorProperty *nvp) {
        // // std::cerr << "new number " << nvp->name << "\n";
        // if (!strcmp(nvp->name, "CCD_INFO")) {
        //     indiFrame.maxWidth = -1;
        //     indiFrame.maxHeight = -1;

        //     for(int i = 0; i < nvp->nnp; ++i) {
        //         auto prop = nvp->np+i;
        //         if (!strcmp(prop->name, "CCD_MAX_X")) {
        //             indiFrame.maxWidth = prop->value;
        //         }
        //         if (!strcmp(prop->name, "CCD_MAX_Y")) {
        //             indiFrame.maxHeight = prop->value;
        //         }
        //     }
        //     std::cerr << "got ccd info " << indiFrame.maxWidth << "x" << indiFrame.maxHeight << "\n";
        // }
        // if (!strcmp(nvp->name,"CCD_FRAME")) {
        //     for(int i = 0; i < nvp->nnp; ++i) {
        //         auto prop = nvp->np+i;
        //         if (!strcmp(prop->name, "X")) {
        //             indiFrame.X = prop->value;
        //         }
        //         if (!strcmp(prop->name, "Y")) {
        //             indiFrame.Y = prop->value;
        //         }
        //         if (!strcmp(prop->name, "WIDTH")) {
        //             indiFrame.W = prop->value;
        //         }
        //         if (!strcmp(prop->name, "HEIGHT")) {
        //             indiFrame.H = prop->value;
        //         }
        //     }
        //     std::cerr << "got ccd frame " << indiFrame.W << "x" << indiFrame.H << "@("<< indiFrame.X << "," << indiFrame.Y << ")\n";
        // }
        // if (!strcmp(nvp->name,"CCD_BINNING")) {
        //     indiFrame.vbin = 1;
        //     indiFrame.hbin = 1;
        //     for(int i = 0; i < nvp->nnp; ++i) {
        //         auto prop = nvp->np+i;
        //         if (!strcmp(prop->name, "HOR_BIN")) {
        //             indiFrame.hbin = prop->value;
        //             if (indiFrame.hbin < 1) {
        //                 indiFrame.hbin = 1;
        //             }
        //         }
        //         if (!strcmp(prop->name, "VER_BIN")) {
        //             indiFrame.vbin = prop->value;
        //             if (indiFrame.vbin < 1) {
        //                 indiFrame.vbin = 1;
        //             }
        //         }
        //     }
        //     std::cerr << "got ccd bin " << indiFrame.hbin << "x" << indiFrame.vbin << "\n";
        // }
    }

    virtual void newMessage(INDI::BaseDevice *dp, int messageID) {
        // std::cerr << "new message\n";
    }
    virtual void newText(ITextVectorProperty *tvp) {
        // std::cerr << "new text\n";
    }
    virtual void newLight(ILightVectorProperty *lvp) {
        // std::cerr << "new light\n";
    }
    virtual void serverConnected() {
        std::cerr << "server connected\n";
    }
    virtual void serverDisconnected(int exit_code) {
        std::cerr << "server disconnected\n";
    }
private:
   INDI::BaseDevice * ccd_simulator;
};

TEST(IndiclientTcpConnect, ClientConnect) {
    ServerMock fakeServer;
    IndiClientMock indiServerCnx;

    setupSigPipe();

    fakeServer.listen(TEST_TCP_PORT);

    MyClient * client = new MyClient("machin", "truc");
    client->setServer("127.0.0.1", TEST_TCP_PORT);



    // FIXME : async
    // auto wait = runAsync([&fakeServer]=>{
    std::thread t1([&fakeServer, &indiServerCnx]() {
        fakeServer.accept(indiServerCnx);
        indiServerCnx.cnx.expectXml("<getProperties version='1.7'/>");
    });
    // });
    bool connected = client->connectServer();
    ASSERT_EQ(connected, true);

    t1.join();

    // Check client reply to ping...
    // indiServerCnx.cnx.send("<pingRequest uuid='123456'/>");
    // indiServerCnx.cnx.expectXml("<pingReply uuid='123456'/>");
}
