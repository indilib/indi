#include "testclient.h"

#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#define CAMNAME "CcdSimulator"

testclient::testclient()
{
    //ctor
    Connected=false;
    mycam=NULL;
}

testclient::~testclient()
{
    //dtor
}

void testclient::newDevice(const char *device_name)
{
    printf("Got a new device %s\n",device_name);
    if(strcmp(device_name,CAMNAME)==0) {
        printf("This is our camera, get a device pointer\n");
        mycam=getDriver(CAMNAME);
    }
    return;
}

void testclient::newProperty(const char *device_name, const char *property_name)
{
    printf("Got a new property vector %s\n",property_name);
    return;
}

void testclient::newBLOB(IBLOB *b)
{
    //printf("Testclient got a blob %x\n",b);
    printf("testclient got a blob type '%s' '%s' '%s'\n",b->name,b->label,b->format);
    gotblob++;
    return;
}

void testclient::newSwitch(ISwitchVectorProperty *sw)
{
    //printf("got switch vector property for %s\n",sw->name );
    return;
}

void testclient::newNumber(INumberVectorProperty *p)
{
    //printf("got number vector property %s\n",p->name);
    return;
}

void testclient::newText(ITextVectorProperty *)
{
    printf("Got text vector property\n");
    return;
}

void testclient::newLight(ILightVectorProperty *)
{
    printf("got light vector property\n");
    return;
}

void testclient::serverConnected()
{
    printf("Server Connected\n");
    Connected=true;
    return;
}

void testclient::serverDisconnected()
{
    printf("server Disconnected\n");
    Connected=false;
    return;
}

void testclient::setnumber(char *p,float v)
{
        INumberVectorProperty *exptime;
        exptime=mycam->getNumber(p);
        if(exptime ==NULL) {
            printf("Error, cannot find %s property vector\n",p);
            exit(-1);
        }
        exptime->np[0].value=v;
        sendNewNumber(exptime);

}

char * testclient::PrintProperties(char *n)
{
    IBLOBVectorProperty *bv;
    bv=mycam->getBLOB(n);
    char *propret;
    //printf("getBlob produces %08x\n",bv);
    if(bv != NULL) {
        printf("Blob Vector has %d properties device '%s' name '%s' label '%s'\n",bv->nbp,bv->device,bv->name,bv->label);
        for(int x=0; x<bv->nbp; x++) {
            printf("Property has name '%s' label '%s'\n",bv->bp[x].name,bv->bp[x].label);
            propret=bv->bp[x].name;
        }
    }

    return propret;

}
int testclient::dotest()
{
    //INDI::BaseDriver * mycam;

    //mycam=getDriver("SxCamera");
    if(mycam != NULL) {
        int spin;
        char *p;

        printf("working with %s\n",mycam->deviceName());

        printf("Tell basedriver to connect to the camera\n");
        setDriverConnection(true,CAMNAME);
        //  Wait for a bit, driver is going to populate more items
        sleep(3);
        //  these are the two I am interested in
        printf("Printing vectors and properties for our image blobs\n");
        p=PrintProperties((char *)"CCD1");
        printf("First blob is %s\n",p);

        printf("Calling setBLOBMode B_ALSO for %s with %s\n",CAMNAME,p);
        setBLOBMode(B_ALSO,CAMNAME,p);

        p=PrintProperties((char *)"CCD2");
        printf("Second blob is %s\n",p);
        spin=0;gotblob=0;
        printf("Setting exposure, this should produce one blob\n");
        setnumber((char *)"CCD_EXPOSURE",1.0);
        while((gotblob<2)&&(spin++ < 10)) {
            sleep(1);
        }
        printf("Got %d blobs \n",gotblob);
        //  Set with empty string because null segfaults
        printf("Calling setBLOBMode B_ALSO for %s with \"\"\n",CAMNAME);
        setBLOBMode(B_ALSO,CAMNAME,"");
        printf("Setting exposure, this should produce two blobs\n");
        spin=0;gotblob=0;
        setnumber((char *)"CCD_EXPOSURE",1.0);
        setnumber((char *)"GUIDER_EXPOSURE",1.0);
        while((gotblob<2)&&(spin++ < 10)) {
            sleep(1);
        }
        printf("Got %d blobs \n",gotblob);


        printf("Calling setBLOBMode B_ALSO for %s with \"\"\n",CAMNAME);
        setBLOBMode(B_NEVER,CAMNAME,"");
        printf("Setting exposure, this should produce ZERO blobs\n");
        spin=0;gotblob=0;
        setnumber((char *)"CCD_EXPOSURE",1.0);
        setnumber((char *)"GUIDER_EXPOSURE",1.0);
        while((gotblob<2)&&(spin++ < 10)) {
            sleep(1);
        }
        printf("Got %d blobs \n",gotblob);

        printf("Calling setBLOBMode B_ALSO for %s with %s\n",CAMNAME,p);
        setBLOBMode(B_ALSO,CAMNAME,p);

        p=PrintProperties((char *)"CCD2");
        printf("Second blob is %s\n",p);
        spin=0;gotblob=0;
        printf("Setting exposure, this should produce one blob\n");
        setnumber((char *)"CCD_EXPOSURE",1.0);
        setnumber((char *)"GUIDER_EXPOSURE",1.0);
        while((gotblob<2)&&(spin++ < 10)) {
            sleep(1);
        }
        printf("Got %d blobs \n",gotblob);


        /*
        if(gotblob==1) {
            printf("Got this blob like we should have\n");
        } else {
            printf("Did not get the blob we should have got\n");
        }
        printf("Setting ccd exposure, this should produce a driver blob that we dont get\n");
        spin=0;gotblob=0;
        setnumber((char *)"CCD_EXPOSURE",0.1);
        while((gotblob==0)&&(spin++ < 10)) {
            sleep(1);
        }
        if(gotblob==1) {
            printf("Got this blob that we shouldn't have got\n");
        } else {
            printf("Did not get the blob we shouldn't have got\n");
        }
        */

    }
    return 0;
}
