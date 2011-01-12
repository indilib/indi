#include <stdio.h>
#include <stdlib.h>

#include "testclient.h"

int WriteFifo(const char *t)
{
    FILE *fifo;

    fifo=fopen("/tmp/indi","w");
    if(fifo != NULL) {
        fprintf(fifo,"%s\n",t);
        fclose(fifo);
        return 0;
    }
    return -1;
}

int StartIndiServer()
{
    int pid;
    pid=fork();
    if(pid==0) {
        //  this is the child
        //  and this should never return
        execlp("indiserver","indiserver","-f","/tmp/indi",NULL);
        fprintf(stderr,"Error spawning indiserver\n");
        exit(-1);

    }
    return pid;
}

int main()
{
    bool rc;
    testclient *client;
    int serverpid;

    //  first we need to make sure we have a fifo
    system("mkfifo /tmp/indi");
    //  now we want to start the indi server listening on the fifo
    //  and running in parallel
    //serverpid=StartIndiServer();
    //printf("Server pid is %d\n",serverpid);
    //  give the server a chance to get started
    sleep(2);
    //  now we need to start the scope driver
    WriteFifo("start indi_scopesim TestScope");
    //  and lets start a ccd
    WriteFifo("start indi_ccdsim TestCcd");
    //  and lets start another ccd
    WriteFifo("start indi_ccdsim TestCcd2");

    client=new testclient();

    client->setServer("localhost",7624);
    rc=client->connectServer();
    sleep(5);  //  liet it connect
    //if(rc) client->dotest();
    //printf("program exit\n");

    client->connectscope(true);
    client->connectcam(true);

    sleep(5);

    //  ok, we are connected, now stop the camera

    WriteFifo("stop indi_ccdsim TestCcd");
    sleep(2);

    //  Disconnect, and get a fault in the client library somewhere
    printf("Calling disconnect\n");
    client->disconnectServer();

    sleep(5);
    if(client->Connected) {
        printf("No Disconnect messages came to the client\n");
    }

    //  now reconnect to the server
    //  And this will cause the server to abend with a stack dump
    rc=client->connectServer();

    //  now wait and see what happens
    sleep(20);


    delete client;  // we want to clean up, but, this abends inthe library somwhere
    return 0;
}
